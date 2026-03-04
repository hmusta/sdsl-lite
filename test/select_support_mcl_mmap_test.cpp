#include "sdsl/bit_vectors.hpp"
#include "sdsl/select_support.hpp"
#include "sdsl/io.hpp"
#include "sdsl/memory_management.hpp"
#include "gtest/gtest.h"
#include <string>
#include <vector>

using namespace sdsl;
using namespace std;

string temp_dir;

namespace
{

template<class T>
class select_support_mcl_mmap_test : public ::testing::Test { };

using testing::Types;

typedef Types<select_support_mcl<>,
        select_support_mcl<0>,
        select_support_mcl<01,2>,
        select_support_mcl<10,2>
        > Implementations;

TYPED_TEST_CASE(select_support_mcl_mmap_test, Implementations);

// Helper: count pattern matches and collect expected select() results
template<class T>
vector<uint64_t> collect_select_answers(const bit_vector& bv)
{
    vector<uint64_t> answers;
    for (uint64_t j = 0; j < bv.size(); ++j) {
        bool found = (j >= T::bit_pat_len - 1);
        for (uint8_t k = 0; found && k < T::bit_pat_len; ++k) {
            found &= bv[j - k] == ((T::bit_pat >> k) & 1);
        }
        if (found) {
            answers.push_back(j);
        }
    }
    return answers;
}

// Build a bit_vector with enough structure to exercise both short and long superblocks
bit_vector make_test_bitvector()
{
    // ~100k bits — enough to produce several superblocks (4096 args each)
    const uint64_t n = 100000;
    bit_vector bv(n, 0);
    // Dense region: every 3rd bit set
    for (uint64_t i = 0; i < n / 2; i += 3) {
        bv[i] = 1;
    }
    // Sparse region: every 1000th bit set
    for (uint64_t i = n / 2; i < n; i += 1000) {
        bv[i] = 1;
    }
    return bv;
}

//! Load via mmap and verify all select() results match the original
TYPED_TEST(select_support_mcl_mmap_test, mmap_load_and_select)
{
    bit_vector bv = make_test_bitvector();
    typename TypeParam::bit_vector_type bv_orig(bv);
    TypeParam ss_orig(&bv_orig);

    auto answers = collect_select_answers<TypeParam>(bv);
    if (answers.empty()) return;

    // Verify original works
    for (uint64_t i = 0; i < answers.size(); ++i) {
        ASSERT_EQ(answers[i], ss_orig.select(i + 1));
    }

    // Serialize bit_vector and select_support to the same file
    string file = temp_dir + "/select_mcl_mmap_test.sdsl";
    {
        osfstream out(file, ios::binary | ios::trunc | ios::out);
        bv_orig.serialize(out);
        ss_orig.serialize(out);
        out.close();
    }

    // Load via mmap_ifstream
    typename TypeParam::bit_vector_type bv_mmap;
    TypeParam ss_mmap;
    {
        mmap_ifstream in(file, ios::binary | ios::in);
        ASSERT_TRUE(in.good());
        bv_mmap.load(in);
        ss_mmap.load(in, &bv_mmap);
    }

    // Verify all select() results
    for (uint64_t i = 0; i < answers.size(); ++i) {
        ASSERT_EQ(answers[i], ss_mmap.select(i + 1))
            << "mismatch at select(" << (i + 1) << ")";
    }

    remove(file);
}

//! Load via mmap, re-serialize, load via normal ifstream, verify
TYPED_TEST(select_support_mcl_mmap_test, mmap_serialize_roundtrip)
{
    bit_vector bv = make_test_bitvector();
    typename TypeParam::bit_vector_type bv_orig(bv);
    TypeParam ss_orig(&bv_orig);

    auto answers = collect_select_answers<TypeParam>(bv);
    if (answers.empty()) return;

    string file1 = temp_dir + "/select_mcl_mmap_rt1.sdsl";
    string file2 = temp_dir + "/select_mcl_mmap_rt2.sdsl";

    // Serialize original
    {
        osfstream out(file1, ios::binary | ios::trunc | ios::out);
        bv_orig.serialize(out);
        ss_orig.serialize(out);
    }

    // Load via mmap
    typename TypeParam::bit_vector_type bv_mmap;
    TypeParam ss_mmap;
    {
        mmap_ifstream in(file1, ios::binary | ios::in);
        ASSERT_TRUE(in.good());
        bv_mmap.load(in);
        ss_mmap.load(in, &bv_mmap);
    }

    // Re-serialize the mmap-loaded object
    {
        osfstream out(file2, ios::binary | ios::trunc | ios::out);
        bv_mmap.serialize(out);
        ss_mmap.serialize(out);
    }

    // Load from the second file via normal ifstream
    typename TypeParam::bit_vector_type bv_normal;
    TypeParam ss_normal;
    {
        isfstream in(file2, ios::binary | ios::in);
        ASSERT_TRUE(in.good());
        bv_normal.load(in);
        ss_normal.load(in, &bv_normal);
    }

    // Verify all select() results
    for (uint64_t i = 0; i < answers.size(); ++i) {
        ASSERT_EQ(answers[i], ss_normal.select(i + 1))
            << "roundtrip mismatch at select(" << (i + 1) << ")";
    }

    remove(file1);
    remove(file2);
}

//! Test with empty bit_vector (no arguments to select)
TYPED_TEST(select_support_mcl_mmap_test, mmap_load_empty)
{
    bit_vector bv(0);
    typename TypeParam::bit_vector_type bv_orig(bv);
    TypeParam ss_orig(&bv_orig);

    string file = temp_dir + "/select_mcl_mmap_empty.sdsl";
    {
        osfstream out(file, ios::binary | ios::trunc | ios::out);
        bv_orig.serialize(out);
        ss_orig.serialize(out);
    }

    typename TypeParam::bit_vector_type bv_mmap;
    TypeParam ss_mmap;
    {
        mmap_ifstream in(file, ios::binary | ios::in);
        ASSERT_TRUE(in.good());
        bv_mmap.load(in);
        ss_mmap.load(in, &bv_mmap);
    }

    ASSERT_EQ(bv_orig.size(), bv_mmap.size());

    remove(file);
}

}// end namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " tmp_dir" << endl;
        return 1;
    }
    temp_dir = argv[1];
    return RUN_ALL_TESTS();
}
