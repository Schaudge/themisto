#pragma once

#include "sdsl/bit_vectors.hpp"
#include "delta_vector.hh"

class Bitmap_Or_Deltas_ColorSet{
    
private:

    // Stores the intersection into buf1 and returns the number of elements in the
    // intersection (does not resize buf1). Buffer elements must be sorted.
    // Assumes all elements in a buffer are distinct
    int64_t intersect_buffers(vector<int64_t>& buf1, int64_t buf1_len, vector<int64_t>& buf2, int64_t buf2_len) const{

        int64_t i = 0, j = 0, k = 0;
        while(i < buf1_len && j < buf2_len){
            if(buf1[i] < buf2[j]) i++;
            else if(buf1[i] > buf2[j]) j++;
            else{
                buf1[k] = buf1[i];
                i++; j++; k++;
            }
        }
        return k;
    }

    // Stores the union into result_buf and returns the number of elements in the
    // union (does not resize result_buf). Buffers elements must be sorted.
    // Assumes all elements in a buffer are distinct. result_buf must have enough
    // space to accommodate the union
    int64_t union_buffers(vector<int64_t>& buf1, int64_t buf1_len, vector<int64_t>& buf2, int64_t buf2_len, vector<int64_t>& result_buf) const{

        auto end = std::set_union(
                        buf1.begin(), buf1.begin() + buf1_len,
                        buf2.begin(), buf2.begin() + buf2_len,
                        result_buf.begin()
        );
        return end - result_buf.begin();
    }

    Bitmap_Or_Deltas_ColorSet(const sdsl::bit_vector& bits) : is_bitmap(true) {
        bitmap = bits;
    }

    Bitmap_Or_Deltas_ColorSet(const Fixed_Width_Delta_Vector& elements) : is_bitmap(false) {
        element_array = elements;
    }


public:

    bool is_bitmap; // Is encoded as a bitmap or with gap encoding?

    sdsl::bit_vector bitmap;

    Fixed_Width_Delta_Vector element_array; // Todo: possibility of encoding deltas between non-existent colors

    Bitmap_Or_Deltas_ColorSet() : is_bitmap(false){}

    // Delete these constructors because they would be implicitly converted into an
    // Delta_Vector and then that constructor would be called, which we don't want
    Bitmap_Or_Deltas_ColorSet(const vector<std::uint64_t>& colors) = delete;
    Bitmap_Or_Deltas_ColorSet(const vector<std::int32_t>& colors) = delete;
    Bitmap_Or_Deltas_ColorSet(const vector<std::uint32_t>& colors) = delete;
    Bitmap_Or_Deltas_ColorSet(const vector<std::int16_t>& colors) = delete;
    Bitmap_Or_Deltas_ColorSet(const vector<std::uint16_t>& colors) = delete;
    Bitmap_Or_Deltas_ColorSet(const vector<std::int8_t>& colors) = delete;
    Bitmap_Or_Deltas_ColorSet(const vector<std::uint8_t>& colors) = delete;

    Bitmap_Or_Deltas_ColorSet(const vector<std::int64_t>& colors) {
        int64_t max_color = *std::max_element(colors.begin(), colors.end());
        
        Fixed_Width_Delta_Vector size_test(colors);
        if(colors.size() > 0 && size_test.size_in_bytes()*8 > max_color+1){
            // Bitmap is smaller
            // Empty sets are never encoded as bit maps because we want a constant-time
            // check for whether the set is empty.
            is_bitmap = true;
            sdsl::bit_vector bv(max_color+1, 0);
            for(int64_t x : colors) bv[x] = 1;
            bitmap = bv;
        } else{
            // Delta array is smaller (or set is empty)
            is_bitmap = false;
            element_array = size_test;
        }
    }

    std::vector<int64_t> get_colors_as_vector() const {
        std::vector<int64_t> vec;
        if(is_bitmap){    
            for(int64_t i = 0; i < bitmap.size(); i++){
                if(bitmap[i]) vec.push_back(i);
            }
        } else{
            return element_array.get_values();
        }
        return vec;
    }

    void get_colors_as_vector(std::vector<int64_t>& vec) const{
        // Todo: implement without get_colors_as_vector
        for(int64_t color : get_colors_as_vector())
            vec.push_back(color);
    }


    bool empty() const{
        if(is_bitmap) return false; // Empty sets are always encoded as sparse
        else return element_array.empty();
    }

    // Warning: THIS TAKES LINEAR TIME. If you just need to know if the set is
    // empty, call empty()
    int64_t size() const {
        int64_t count = 0;
        if(is_bitmap){
            for(bool b : bitmap) count += b;
        }
        else {
            count += element_array.get_values().size();
        }
        return count;
    }


    int64_t size_in_bits() const {
        return (sizeof(is_bitmap) + sdsl::size_in_bytes(bitmap) + element_array.size_in_bytes()) * 8;
    }

    // This is O(1) time for dense color sets but O(set size) for sparse sets.
    bool contains(const int64_t color) const {
        if(color < 0) throw std::runtime_error("Called Color Set contains-method with a negative color id");
        if(is_bitmap) return color < bitmap.size() && bitmap[color];
        else{
            vector<int64_t> values = element_array.get_values();
            return std::find(values.begin(), values.end(), color) != values.end();
        }
    }

    sdsl::bit_vector bitmap_vs_bitmap_intersection(const sdsl::bit_vector& A, const sdsl::bit_vector& B) const{
        int64_t n = min(A.size(), B.size());
        sdsl::bit_vector result(n, 0);
        int64_t words = n / 64;

        // Do 64-bit bitwise ands
        for(int64_t w = 0; w < words; w++){
            result.set_int(w*64, A.get_int(w*64) & B.get_int(w*64));
        }

        // Do the rest one bit at a time
        for(int64_t i = words*64; i < n; i++){
            result[i] = A[i] && B[i];
        }

        return result;    
    }

    sdsl::bit_vector bitmap_vs_bitmap_union(const sdsl::bit_vector& A, const sdsl::bit_vector& B) const{
        int64_t n = max(A.size(), B.size()); // Length of union
        sdsl::bit_vector result(n, 0);

        int64_t words = min(A.size(), B.size()) / 64; // Number of 64-bit words common to A and B

        // Do 64-bit bitwise ors
        for(int64_t w = 0; w < words; w++){
            result.set_int(w*64, A.get_int(w*64) | B.get_int(w*64));
        }

        // Do the rest one bit at a time
        for(int64_t i = words*64; i < n; i++){
            result[i] = (i < A.size() && A[i]) || (i < B.size() && B[i]);
        }

        return result;
    }

    Fixed_Width_Delta_Vector bitmap_vs_element_array_intersection(const sdsl::bit_vector& bm, const Fixed_Width_Delta_Vector& ea) const{
        vector<int64_t> new_elements;
        for(int64_t x : ea.get_values()){
            if(x >= bm.size()) break;
            if(bm[x] == 1) new_elements.push_back(x);
        }
        return Fixed_Width_Delta_Vector(new_elements);
    }

    sdsl::bit_vector bitmap_vs_element_array_union(const sdsl::bit_vector& bm, const Fixed_Width_Delta_Vector& ea) const{
        if(ea.empty()) return bm;

        // Decode the integers in the element array
        vector<int64_t> elements;
        for(int64_t x : ea.get_values()) elements.push_back(x);

        // Allocate space for the union
        int64_t union_size = max(elements.back()+1, (int64_t)bm.size());
        sdsl::bit_vector result(union_size, 0);

        // Add the bit map
        for(int64_t i = 0; i < bm.size(); i++) result[i] = bm[i];

        // Add the elements
        for(int64_t x : elements) result[x] = 1;

        return result;
    }    

    Fixed_Width_Delta_Vector element_array_vs_element_array_intersection(const Fixed_Width_Delta_Vector& A, const Fixed_Width_Delta_Vector& B) const{
        vector<int64_t> A_vec = A.get_values();
        vector<int64_t> B_vec = B.get_values();
        int64_t size = intersect_buffers(A_vec, A_vec.size(), B_vec, B_vec.size());
        A_vec.resize(size);
        return Fixed_Width_Delta_Vector(A_vec);
    }

    Fixed_Width_Delta_Vector element_array_vs_element_array_union(const Fixed_Width_Delta_Vector& A, const Fixed_Width_Delta_Vector& B) const{
        vector<int64_t> A_vec = A.get_values();
        vector<int64_t> B_vec = B.get_values();
        vector<int64_t> AB_vec(A_vec.size() + B_vec.size()); // Output buffer
        int64_t size = union_buffers(A_vec, A_vec.size(), B_vec, B_vec.size(), AB_vec);
        AB_vec.resize(size);
        return Fixed_Width_Delta_Vector(AB_vec);
    }

    Bitmap_Or_Deltas_ColorSet intersection(const Bitmap_Or_Deltas_ColorSet& c) const {
        if(is_bitmap && c.is_bitmap){
            return Bitmap_Or_Deltas_ColorSet(bitmap_vs_bitmap_intersection(bitmap, c.bitmap));
        } else if(is_bitmap && !c.is_bitmap){
            return Bitmap_Or_Deltas_ColorSet(bitmap_vs_element_array_intersection(bitmap, c.element_array));
        } else if(!is_bitmap && c.is_bitmap){
            return Bitmap_Or_Deltas_ColorSet(bitmap_vs_element_array_intersection(c.bitmap, element_array));
        } else{ // Element array vs element array
            return Bitmap_Or_Deltas_ColorSet(element_array_vs_element_array_intersection(element_array, c.element_array));
        }
    }

    // union is a reserved word in C++ so this function is called do_union
    Bitmap_Or_Deltas_ColorSet do_union(const Bitmap_Or_Deltas_ColorSet& c) const {
        if(is_bitmap && c.is_bitmap){
            return Bitmap_Or_Deltas_ColorSet(bitmap_vs_bitmap_union(bitmap, c.bitmap));
        } else if(is_bitmap && !c.is_bitmap){
            return Bitmap_Or_Deltas_ColorSet(bitmap_vs_element_array_union(bitmap, c.element_array));
        } else if(!is_bitmap && c.is_bitmap){
            return Bitmap_Or_Deltas_ColorSet(bitmap_vs_element_array_union(c.bitmap, element_array));
        } else{ // Element array vs element array
            return Bitmap_Or_Deltas_ColorSet(element_array_vs_element_array_union(element_array, c.element_array));
        }
    }

    int64_t serialize(std::ostream& os) const {
        int64_t n_bytes_written = 0;

        char flag = is_bitmap;
        os.write(&flag, 1);
        n_bytes_written  += 1;

        n_bytes_written += bitmap.serialize(os);
        n_bytes_written += element_array.serialize(os);

        return n_bytes_written;
    }

    void load(std::istream& is) {
        char flag;
        is.read(&flag, 1);
        is_bitmap = flag;
        bitmap.load(is);
        element_array.load(is);
    }

};
