#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
#include "snarkfront.hpp"

using namespace snarkfront;
using namespace std;

void printUsage(const char* exeName) {
    cout << "usage: " << exeName
         << " -p BN128|Edwards"
            " -b 256|512"
            " -d tree_depth"
            " -i leaf_number"
         << endl;

    exit(EXIT_FAILURE);
}

template <typename PAIRING, typename BUNDLE, typename ZK_PATH>
void runTest(const size_t treeDepth,
             const size_t leafNumber)
{
    BUNDLE bundle(treeDepth);

    while (! bundle.isFull()) {
        const typename BUNDLE::DigType leaf{bundle.treeSize()};

        bundle.addLeaf(
            leaf,
            leafNumber == bundle.treeSize());
    }

    if (leafNumber >= bundle.treeSize()) {
        cout << "leaf number " << leafNumber
             << " is larger than " << bundle.treeSize()
             << endl;

        exit(EXIT_FAILURE);
    }

    const auto& leaf = bundle.authLeaf().front();
    const auto& authPath = bundle.authPath().front();

    cout << "leaf " << leafNumber << " child bits ";
    for (int i = authPath.childBits().size() - 1; i >= 0; --i) {
        cout << authPath.childBits()[i];
    }
    cout << endl;

    cout << "root path" << endl;
    for (int i = authPath.rootPath().size() - 1; i >= 0; --i) {
        cout << "[" << i << "] "
             << asciiHex(authPath.rootPath()[i], true) << endl;
    }

    cout << "siblings" << endl;
    for (int i = authPath.siblings().size() - 1; i >= 0; --i) {
        cout << "[" << i << "] "
             << asciiHex(authPath.siblings()[i], true) << endl;
    }

    typename ZK_PATH::DigType rt;
    bless(rt, authPath.rootHash());

    end_input<PAIRING>();

    typename ZK_PATH::DigType zkLeaf;
    bless(zkLeaf, leaf);

    ZK_PATH zkAuthPath(authPath);
    zkAuthPath.updatePath(zkLeaf);

    assert_true(rt == zkAuthPath.rootHash());

    cout << "variable count " << variable_count<PAIRING>() << endl;
}

template <typename PAIRING>
bool runTest(const string& shaBits,
             const size_t treeDepth,
             const size_t leafNumber)
{
    typedef typename PAIRING::Fr FR;

    if ("256" == shaBits) {
        runTest<PAIRING,
                MerkleBundle_SHA256<uint32_t>, // count could be size_t
                zk::MerkleAuthPath_SHA256<FR>>(
            treeDepth,
            leafNumber);

    } else if ("512" == shaBits) {
        runTest<PAIRING,
                MerkleBundle_SHA512<uint64_t>, // count could be size_t
                zk::MerkleAuthPath_SHA512<FR>>(
            treeDepth,
            leafNumber);
    }

    GenericProgressBar progress1(cerr), progress2(cerr, 100);

    cerr << "generate key pair";
    const auto key = keypair<PAIRING>(progress2);
    cerr << endl;

    const auto in = input<PAIRING>();

    cerr << "generate proof";
    const auto p = proof(key, progress2);
    cerr << endl;

    cerr << "verify proof ";
    const bool proofOK = verify(key, in, p, progress1);
    cerr << endl;

    return proofOK;
}

int main(int argc, char *argv[])
{
    // command line switches
    string pairing, shaBits;
    size_t treeDepth = -1, leafNumber = -1;
    int opt;
    while (-1 != (opt = getopt(argc, argv, "p:b:d:i:"))) {
        switch (opt) {
        case ('p') :
            pairing = optarg;
            break;
        case ('b') :
            shaBits = optarg;
            break;
        case('d') : {
                stringstream ss(optarg);
                ss >> treeDepth;
                if (!ss) printUsage(argv[0]);
            }
            break;
        case('i') : {
                stringstream ss(optarg);
                ss >> leafNumber;
                if (!ss) printUsage(argv[0]);
            }
            break;
        }
    }

    if (!validPairingName(pairing) || shaBits.empty() || -1 == treeDepth || -1 == leafNumber)
        printUsage(argv[0]);

    bool result;

    if (pairingBN128(pairing)) {
        // Barreto-Naehrig 128 bits
        init_BN128();
        result = runTest<BN128_PAIRING>(shaBits, treeDepth, leafNumber);

    } else if (pairingEdwards(pairing)) {
        // Edwards 80 bits
        init_Edwards();
        result = runTest<EDWARDS_PAIRING>(shaBits, treeDepth, leafNumber);

    }

    cout << "proof verification " << (result ? "OK" : "FAIL") << endl;

    exit(EXIT_SUCCESS);
}
