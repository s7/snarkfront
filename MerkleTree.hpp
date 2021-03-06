#ifndef _SNARKFRONT_MERKLE_TREE_HPP_
#define _SNARKFRONT_MERKLE_TREE_HPP_

#include <array>
#include <cstdint>
#include <iostream>
#include <istream>
#include <ostream>
#include <set>
#include <vector>
#include "DSL_base.hpp"
#include "DSL_bless.hpp"
#include "DSL_utility.hpp"
#include "PowersOf2.hpp"
#include "SHA_256.hpp"
#include "SHA_512.hpp"

namespace snarkfront {

////////////////////////////////////////////////////////////////////////////////
// authentication path from a binary Merkle tree
//

template <typename HASH, typename BIT>
class MerkleAuthPath
{
public:
    typedef HASH HashType;
    typedef typename HASH::DigType DigType;

    MerkleAuthPath()
        : m_depth(0)
    {}

    // eval
    MerkleAuthPath(const std::size_t depth)
        : m_depth(depth),
          m_rootPath(depth) // first update initializes hash digests
    {
        m_siblings.reserve(depth);
        m_childBits.reserve(depth);

        for (std::size_t i = 0; i < depth; ++i) {
            m_siblings.emplace_back(zero());
            m_childBits.emplace_back(0);
        }
    }

    // zk from eval
    template <typename OTHER_HASH, typename OTHER_BIT>
    MerkleAuthPath(const MerkleAuthPath<OTHER_HASH, OTHER_BIT>& other)
        : m_depth(other.depth()),
          m_rootPath(other.depth()) // first update initializes hash digests
    {
        m_siblings.reserve(other.depth());
        for (const auto& a : other.siblings()) {
            DigType b;
            bless(b, a);
            m_siblings.emplace_back(b);
        }

        m_childBits.reserve(other.depth());
        for (const auto& a : other.childBits()) {
            BIT b;
            bless(b, a);
            m_childBits.emplace_back(b);
        }
    }

    std::size_t depth() const {
        return m_depth;
    }

    const DigType& rootHash() const {
        return m_rootPath.back();
    }

    // bottom-up order, index 0 is at the leaves of tree
    const std::vector<DigType>& rootPath() const { return m_rootPath; }
    const std::vector<DigType>& siblings() const { return m_siblings; }
    const std::vector<BIT>& childBits() const { return m_childBits; }

    // update hash codes along path back to root
    void updatePath(const DigType& leaf) {
        std::vector<MerkleAuthPath> dummy;
        updatePath(leaf, dummy);
    }

    // update hash codes along path back to root
    void updatePath(const DigType& leaf,
                    std::vector<MerkleAuthPath>& oldPaths)
    {
        // root path overlap
        std::vector<int> overlap;
        overlap.reserve(oldPaths.size());
        for (const auto& a : oldPaths) {
            overlap.push_back(
                matchMSB(m_childBits, a.childBits()));
        }

        HASH hashAlgo;

        auto dig = leaf;

        // ascend tree from leaf to root
        for (std::size_t i = 0; i < m_depth; ++i) {
            hashAlgo.clearMessage();

            const auto& isRightChild = m_childBits[i];
            const auto
                leftDigest = ternary(isRightChild, m_siblings[i], dig),
                rightDigest = ternary(isRightChild, dig, m_siblings[i]);

            hashAlgo.msgInput(leftDigest);
            hashAlgo.msgInput(rightDigest);
            hashAlgo.computeHash();

            dig = m_rootPath[i] = hashAlgo.digest();

            // path length from root to node with the new hash
            const int pathLen = m_depth - 1 - i;

            // update old authentication paths
            for (std::size_t j = 0; j < overlap.size(); ++j) {
                if (pathLen <= overlap[j]) {
                    // update root path hashes
                    oldPaths[j].m_rootPath[i] = dig;

                } else if (pathLen == overlap[j] + 1) {
                    // update sibling hashes
                    oldPaths[j].m_siblings[i + 1] = dig;

                }
            }
        }

        // update old authentication paths
        for (std::size_t j = 0; j < overlap.size(); ++j) {
            if (m_depth - 1 == overlap[j])
                // differ in last bit only, leaf must be right sibling
                oldPaths[j].m_siblings[0] = leaf;
        }
    }

    // just added leaf becomes left sibling
    void leafSibling(const DigType& leaf) {
        m_siblings[0] = leaf;
    }

    // new branch in Merkle tree
    void hashSibling(const std::size_t index)
    {
        m_siblings[index] = m_rootPath[index - 1];

        for (std::size_t i = 0; i < index; ++i)
            m_siblings[i] = zero();
    }

    // returns index of first set bit (right child)
    int incChildBits()
    {
        for (std::size_t i = 0; i < m_depth; ++i) {
            auto& a = m_childBits[i];

            if (bool(0 == a)) {
                // bit is zero, increment to one
                a = 1;
                return i;

            } else {
                // bit is one, increment to zero and carry
                a = 0;
            }
        }

        // increment all ones wraps back to zero with carry
        return -1;
    }

    void marshal_out(std::ostream& os) const {
        os << m_depth << std::endl
           << m_rootPath
           << m_siblings;

        for (const auto& a : m_childBits) {
            os << a << std::endl;
        }
    }

    bool marshal_in(std::istream& is) {
        m_depth = 0; // use as valid flag

        std::size_t len = 0;
        is >> len;
        if (!is || 0 == len) return false;

        m_rootPath.resize(len);
        is >> m_rootPath;
        if (!is) return false;

        m_siblings.resize(len);
        is >> m_siblings;
        if (!is) return false;

        m_childBits.resize(len);
        for (auto& r : m_childBits) {
            is >> r;
            if (!is) return false;
        }

        m_depth = len;

        return true;
    }

private:
    // note: not called by proof generation
    static DigType zero() {
        return {0};
    }

    std::size_t m_depth;

    // indices start from 0 at the leaves increasing up to the root
    std::vector<DigType> m_rootPath, m_siblings;

    // next available leaf element
    std::vector<BIT> m_childBits;
};

template <typename HASH, typename BIT>
std::ostream& operator<< (std::ostream& os, const MerkleAuthPath<HASH, BIT>& a) {
    a.marshal_out(os);
    return os;
}

template <typename HASH, typename BIT>
std::istream& operator>> (std::istream& is, MerkleAuthPath<HASH, BIT>& a) {
    a.marshal_in(is);
    return is;
}

////////////////////////////////////////////////////////////////////////////////
// Merkle tree (binary)
//

template <typename HASH>
class MerkleTree
{
public:
    typedef HASH HashType;
    typedef typename HASH::DigType DigType;

    MerkleTree()
        : m_isFull(true),
          m_authPath(0)
    {}

    // unmanaged (eval namespace)
    MerkleTree(const std::size_t depth)
        : m_isFull(false),
          m_authPath(depth)
    {}

    // true when number of occupied leaves is 2^depth
    bool isFull() const {
        return m_isFull;
    }

    const MerkleAuthPath<HASH, int>& authPath() const {
        return m_authPath;
    }

    // update hash codes along path back to root
    void updatePath(const DigType& leaf) {
        m_authPath.updatePath(leaf);
    }

    // update hash codes along path back to root
    void updatePath(const DigType& leaf,
                    std::vector<MerkleAuthPath<HASH, int>>& v)
    {
        m_authPath.updatePath(leaf, v);
    }

    // prepare tree for next leaf
    void updateSiblings(const DigType& leaf)
    {
        // counter for next leaf element
        const int firstBit = m_authPath.incChildBits();

        if (-1 == firstBit) {
            // tree is full
            m_isFull = true;
            return;

        } else if (0 == firstBit) {
            // next leaf is right child
            m_authPath.leafSibling(leaf);

        } else {
            // left sibling of new branch in tree
            m_authPath.hashSibling(firstBit);
        }
    }

    void marshal_out(std::ostream& os) const {
        os << m_isFull << std::endl
           << m_authPath;
    }

    bool marshal_in(std::istream& is) {
        m_isFull = true; // use as valid flag

        bool full = true;
        is >> full;
        if (!is) return false;

        if (! m_authPath.marshal_in(is)) return false;

        m_isFull = full;

        return true;
    }

private:
    bool m_isFull;
    MerkleAuthPath<HASH, int> m_authPath;
};

template <typename HASH>
std::ostream& operator<< (std::ostream& os, const MerkleTree<HASH>& a) {
    a.marshal_out(os);
    return os;
}

template <typename HASH>
std::istream& operator>> (std::istream& is, MerkleTree<HASH>& a) {
    a.marshal_in(is);
    return is;
}

////////////////////////////////////////////////////////////////////////////////
// Merkle tree with authentication paths
//

template <typename TREE, typename PATH, typename COUNT>
class MerkleBundle
{
public:
    typedef typename TREE::HashType HashType;
    typedef typename TREE::DigType DigType;

    MerkleBundle()
        : m_treeSize(0)
    {}

    MerkleBundle(const std::size_t depth)
        : m_tree(depth),
          m_treeSize(0)
    {}

    bool isFull() const {
        return m_tree.isFull();
    }

    COUNT treeSize() const {
        return m_treeSize;
    }

    const DigType& rootHash() const {
        return m_tree.authPath().rootHash();
    }

    void addLeaf(const DigType& cm, const bool keepPath = true) {
        m_tree.updatePath(cm, m_authPath);

        if (keepPath) {
            m_authLeaf.emplace_back(cm);
            m_authPath.emplace_back(m_tree.authPath());
        }

        m_tree.updateSiblings(cm);

        ++m_treeSize;
    }

    const std::vector<DigType>& authLeaf() const {
        return m_authLeaf;
    }

    const std::vector<PATH>& authPath() const {
        return m_authPath;
    }

    void authGarbageCollect(const std::set<HashType>& keepSet) {
        std::vector<DigType> keepLeaf;
        std::vector<PATH> keepPath;

        for (std::size_t i = 0; i < m_authLeaf.size(); ++i) {
            const auto& cm = m_authLeaf[i];
            if (keepSet.count(cm)) {
                keepLeaf.emplace_back(cm);
                keepPath.emplace_back(m_authPath[i]);
            }
        }

        m_authLeaf = keepLeaf;
        m_authPath = keepPath;
    }

    void marshal_out(std::ostream& os) const {
        os << m_tree
           << m_treeSize << std::endl
           << m_authLeaf;

        for (const auto& r : m_authPath)
            os << r;
    }

    bool marshal_in(std::istream& is) {
        if (!m_tree.marshal_in(is) || !(is >> m_treeSize) || !(is >> m_authLeaf))
            return false;

        m_authPath.resize(m_authLeaf.size());
        for (auto& r : m_authPath) {
            if (! r.marshal_in(is)) return false;
        }

        return true;
    }

private:
    TREE m_tree;
    COUNT m_treeSize;

    std::vector<DigType> m_authLeaf;
    std::vector<PATH> m_authPath;
};

template <typename TREE, typename PATH, typename COUNT>
std::ostream& operator<< (std::ostream& os,
                          const MerkleBundle<TREE, PATH, COUNT>& a) {
    a.marshal_out(os);
    return os;
}

template <typename TREE, typename PATH, typename COUNT>
std::istream& operator>> (std::istream& is,
                          MerkleBundle<TREE, PATH, COUNT>& a) {
    a.marshal_in(is);
    return is;
}

////////////////////////////////////////////////////////////////////////////////
// typedefs
//

namespace zk {
    template <typename FR> using MerkleAuthPath_SHA256 = MerkleAuthPath<SHA256<FR>, bool_x<FR>>;
    template <typename FR> using MerkleAuthPath_SHA512 = MerkleAuthPath<SHA512<FR>, bool_x<FR>>;
} // namespace zk

namespace eval {
    typedef MerkleAuthPath<SHA256, int> MerkleAuthPath_SHA256;
    typedef MerkleAuthPath<SHA512, int> MerkleAuthPath_SHA512;
} // namespace eval

typedef MerkleTree<eval::SHA256> MerkleTree_SHA256;
typedef MerkleTree<eval::SHA512> MerkleTree_SHA512;

template <typename COUNT> using
MerkleBundle_SHA256 = MerkleBundle<MerkleTree_SHA256, eval::MerkleAuthPath_SHA256, COUNT>;

template <typename COUNT> using
MerkleBundle_SHA512 = MerkleBundle<MerkleTree_SHA512, eval::MerkleAuthPath_SHA512, COUNT>;

} // namespace snarkfront

#endif
