#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>
#include "snarkfront.hpp"

using namespace snarkfront;
using namespace std;

void printUsage(const char* exeName) {
    cout << "usage: " << exeName
         << " -m keygen|input|proof|verify"
         << endl;

    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    // command line switches
    string mode;
    int opt;
    while (-1 != (opt = getopt(argc, argv, "m:"))) {
        switch (opt) {
        case ('m') :
            mode = optarg;
            break;
        }
    }

    // Barreto-Naehrig 128 bits
    init_BN128();
    typedef BN128_FR FR;
    typedef BN128_PAIRING PAIRING;

    // output hash digest is publicly known
    const auto pubHash = digest(eval::SHA256(), "abc");

    if ("keygen" == mode) {

        ////////////////////////////////////////////////////////////
        // trusted key generation

        // input variables (values don't matter here)
        array<uint32_x<FR>, 8> pubVars;
        bless(pubVars);

        // marks end of public input variables
        end_input<PAIRING>();

        // constraint system from circuit
        assert_true(pubVars == digest(zk::SHA256<FR>(), ""));

        // generate proving/verification key pair
        GenericProgressBar progress(cerr, 50);
        cerr << "generate key pair";
        cout << keypair<PAIRING>(progress); // expensive!
        cerr << endl;

    } else if ("input" == mode) {

        ////////////////////////////////////////////////////////////
        // public inputs

        // input variables (need values)
        array<uint32_x<FR>, 8> pubVars;
        bless(pubVars, pubHash);

        // marks end of public input variables
        end_input<PAIRING>();

        // publicly known input variables
        cout << input<PAIRING>();

    } else if ("proof" == mode) {

        ////////////////////////////////////////////////////////////
        // generate a proof

        Keypair<PAIRING> keypair; // proving/verification key pair
        Input<PAIRING> input;     // public inputs to circuit
        cin >> keypair >> input;

        // check for marshalling errors
        assert(!keypair.empty() && !input.empty());

        // input variables (need values)
        array<uint32_x<FR>, 8> pubVars;
        bless(pubVars, input);

        // marks end of public input variables
        end_input<PAIRING>();

        // perform calculation
        assert_true(pubVars == digest(zk::SHA256<FR>(), "abc"));

        // generate proof
        GenericProgressBar progress(cerr, 50);
        cerr << "generate proof";
        cout << proof(keypair, progress);
        cerr << endl;

    } else if ("verify" == mode) {

        ////////////////////////////////////////////////////////////
        // verify a proof

        Keypair<PAIRING> keypair; // proving/verification key pair
        Input<PAIRING> input;     // public inputs to circuit
        Proof<PAIRING> proof;     // zero knowledge proof
        cin >> keypair >> input >> proof;

        // check for marshalling errors
        assert(!keypair.empty() && !input.empty() && !proof.empty());

        // verify proof
        GenericProgressBar progress(cerr);
        cerr << "verify proof ";
        const bool valid = verify(keypair, input, proof, progress);
        cerr << endl;
        cout << "proof is " << (valid ? "verified" : "rejected") << endl;

    } else {
        // no mode specified
        printUsage(argv[0]);
    }

    exit(EXIT_SUCCESS);
}
