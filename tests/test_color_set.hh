#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <algorithm>
#include <cassert>
#include <set>
#include <unordered_map>
#include <map>
#include <gtest/gtest.h>
#include <cassert>
#include "sdsl_color_set.hh"

using namespace sbwt;

vector<int64_t> get_sparse_example(){
    vector<int64_t> v = {4, 1534, 4003, 8903};
    return v;
}

vector<int64_t> get_dense_example(int64_t gap, int64_t total_length){
    vector<int64_t> v;
    for(int64_t i = 0; i < total_length; i += gap){
        v.push_back(i);
    }
    return v;
}

TEST(TEST_COLOR_SET, sparse){
    vector<int64_t> v = get_sparse_example();
    Bitmap_Or_Deltas_ColorSet cs(v);

    ASSERT_FALSE(cs.is_bitmap);

    vector<int64_t> v2 = cs.get_colors_as_vector();

    ASSERT_EQ(v,v2);

    int64_t n = *std::max_element(v.begin(), v.end());
    for(int64_t x = 0; x <= n + 10; x++){ // +10: test over the boundary too.
        bool found = std::find(v.begin(), v.end(), x) != v.end();
        ASSERT_EQ(cs.contains(x), found);
    }

    ASSERT_EQ(cs.size(), v.size());
}

TEST(TEST_COLOR_SET, dense){
    vector<int64_t> v = get_dense_example(3, 1000);
    Bitmap_Or_Deltas_ColorSet cs(v);
    ASSERT_TRUE(cs.is_bitmap);

    vector<int64_t> v2 = cs.get_colors_as_vector();

    ASSERT_EQ(v,v2);

    for(int64_t i = 0; i < 1000 + 10; i++){
        ASSERT_EQ(cs.contains(i), (i < 1000 && i % 3 == 0));
    }

    ASSERT_EQ(cs.size(), v.size());
}

TEST(TEST_COLOR_SET, sparse_vs_sparse){

    vector<int64_t> v1 = {4, 1534, 4003, 8903};
    vector<int64_t> v2 = {4, 2000, 4003, 5000};

    Bitmap_Or_Deltas_ColorSet c1(v1);
    Bitmap_Or_Deltas_ColorSet c2(v2);

    ASSERT_FALSE(c1.is_bitmap);
    ASSERT_FALSE(c2.is_bitmap);

    vector<int64_t> v12_inter = c1.intersection(c2).get_colors_as_vector();
    vector<int64_t> correct_inter = {4,4003};
    ASSERT_EQ(v12_inter, correct_inter);

    vector<int64_t> v12_union = c1.do_union(c2).get_colors_as_vector();
    vector<int64_t> correct_union = {4, 1534, 2000, 4003, 5000, 8903};
    ASSERT_EQ(v12_union, correct_union);
}


TEST(TEST_COLOR_SET, dense_vs_dense){

    vector<int64_t> v1 = get_dense_example(2, 1000); // Multiples of 2
    vector<int64_t> v2 = get_dense_example(3, 1000); // Multiples of 3

    Bitmap_Or_Deltas_ColorSet c1(v1);
    Bitmap_Or_Deltas_ColorSet c2(v2);

    ASSERT_TRUE(c1.is_bitmap);
    ASSERT_TRUE(c2.is_bitmap);

    vector<int64_t> v12_inter = c1.intersection(c2).get_colors_as_vector();
    vector<int64_t> correct_inter = get_dense_example(6, 1000); // 6 = lcm(2,3)
    ASSERT_EQ(v12_inter, correct_inter);

    vector<int64_t> v12_union = c1.do_union(c2).get_colors_as_vector();
    vector<int64_t> correct_union;
    for(int64_t i = 0; i < 1000; i++){
        if(i % 2 == 0 || i % 3 == 0) correct_union.push_back(i);
    }
    ASSERT_EQ(v12_union, correct_union);
}

TEST(TEST_COLOR_SET, sparse_vs_dense){

    vector<int64_t> v1 = get_dense_example(3, 10000); // Multiples of 3
    vector<int64_t> v2 = {3, 4, 5, 3000, 6001, 9999};

    Bitmap_Or_Deltas_ColorSet c1(v1);
    Bitmap_Or_Deltas_ColorSet c2(v2);

    ASSERT_TRUE(c1.is_bitmap);
    ASSERT_FALSE(c2.is_bitmap);

    vector<int64_t> v12_inter = c1.intersection(c2).get_colors_as_vector();
    vector<int64_t> correct_inter = {3, 3000, 9999};
    ASSERT_EQ(v12_inter, correct_inter);

    vector<int64_t> v12_union = c1.do_union(c2).get_colors_as_vector();
    vector<int64_t> correct_union;
    for(int64_t i = 0; i < 10000; i++){
        if(i % 3 == 0 || std::find(v2.begin(), v2.end(), i) != v2.end()) correct_union.push_back(i);
    }

    ASSERT_EQ(v12_union, correct_union);
}

Bitmap_Or_Deltas_ColorSet to_disk_and_back(Bitmap_Or_Deltas_ColorSet& c){
    string f = get_temp_file_manager().create_filename();
    throwing_ofstream out(f, ios::binary);
    c.serialize(out.stream);
    out.close();

    Bitmap_Or_Deltas_ColorSet c_loaded;
    throwing_ifstream in(f, ios::binary);
    c_loaded.load(in.stream);
    return c_loaded;
}

TEST(TEST_COLOR_SET, dense_serialization){
    vector<int64_t> v = get_dense_example(3, 10000); // Multiples of 3
    Bitmap_Or_Deltas_ColorSet c(v);
    Bitmap_Or_Deltas_ColorSet c2 = to_disk_and_back(c);
    ASSERT_EQ(c2.get_colors_as_vector(), v);
}


TEST(TEST_COLOR_SET, sparse_serialization){
    vector<int64_t> v = get_sparse_example();
    Bitmap_Or_Deltas_ColorSet c(v);
    Bitmap_Or_Deltas_ColorSet c2 = to_disk_and_back(c);
    ASSERT_EQ(c2.get_colors_as_vector(), v);
}