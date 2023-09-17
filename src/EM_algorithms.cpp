#include <string>
#include <vector>
#include <fstream>
#include "sbwt/globals.hh"
#include "EM_sort.hh"
#include "EM_algorithms.hh"

string EM_sort_big_endian_LL_pairs(string infile, int64_t ram_bytes, int64_t key, int64_t n_threads){
    assert(key == 0 || key == 1);
    
    auto cmp = [&](const char* A, const char* B) -> bool{
        int64_t x_1,y_1,x_2,y_2; // compare (x_1, y_1) and (x_2,y_2)
        x_1 = parse_big_endian_LL(A + 0);
        y_1 = parse_big_endian_LL(A + 8);
        x_2 = parse_big_endian_LL(B + 0);
        y_2 = parse_big_endian_LL(B + 8);
        if(key == 0){
            return make_pair(x_1, y_1) < make_pair(x_2, y_2);
        } else{
            return make_pair(y_1, x_1) < make_pair(y_2, x_2);
        }
    };

    string outfile = get_temp_file_manager().create_filename();
    EM_sort_constant_binary(infile, outfile, cmp, ram_bytes, 8+8, n_threads);
    return outfile;
}

string EM_delete_duplicate_LL_pair_records(string infile){
    string outfile = get_temp_file_manager().create_filename();

    seq_io::Buffered_ifstream in(infile, ios::binary);
    seq_io::Buffered_ofstream out(outfile, ios::binary);

    char prev[8+8]; // two long longs
    char cur[8+8]; // two long longs
    
    int64_t record_count = 0;
    while(in.read(cur, 8+8)){

        if(record_count == 0 || memcmp(prev, cur, 8+8) != 0){
            // The first record or different from the previous record
            out.write(cur, 8+8);
        }

        memcpy(prev, cur, 8+8);
        record_count++;
    }

    return outfile;
}

// (node, color) pairs to (node, color list) pairs
string EM_collect_colorsets_binary(string infile){
    string outfile = get_temp_file_manager().create_filename();

    seq_io::Buffered_ifstream in(infile, ios::binary);
    seq_io::Buffered_ofstream out(outfile, ios::binary);

    int64_t active_key = -1;
    vector<int64_t> cur_value_list;

    char buffer[8+8];

    while(true){
        in.read(buffer,8+8);
        if(in.eof()) break;

        int64_t key = parse_big_endian_LL(buffer + 0);
        int64_t value = parse_big_endian_LL(buffer + 8);

        if(key == active_key) cur_value_list.push_back(value);
        else{
            if(active_key != -1){
                sort(cur_value_list.begin(), cur_value_list.end());
                int64_t record_size = 8 * (1 + 1 + cur_value_list.size());
                write_big_endian_LL(out, record_size);
                write_big_endian_LL(out, active_key);
                for(int64_t x : cur_value_list){
                    write_big_endian_LL(out, x);
                }
            }
            active_key = key;
            cur_value_list.clear();
            cur_value_list.push_back(value);
        }
    }

    // Last one
    if(active_key != -1){
        sort(cur_value_list.begin(), cur_value_list.end());
        int64_t record_size = 8 * (1 + 1 + cur_value_list.size());
        write_big_endian_LL(out, record_size);
        write_big_endian_LL(out, active_key);
        for(int64_t x : cur_value_list){
            write_big_endian_LL(out, x);
        }
    }

    return outfile;
}

// Input: records (node, color set), in the format where
// first we have the length of the whole record in bytes
string EM_sort_by_colorsets_binary(string infile, int64_t ram_bytes, int64_t n_threads){

    auto cmp = [&](const char* x, const char* y) -> bool{
        int64_t nx = parse_big_endian_LL(x);
        int64_t ny = parse_big_endian_LL(y);
        int64_t c = memcmp(x + 8 + 8, y + 8 + 8, min(nx-8-8,ny-8-8));
        if(c < 0) return true;
        else if(c > 0) return false;
        else { // c == 0
            return nx < ny;
        }
    };
    
    string outfile = get_temp_file_manager().create_filename();
    EM_sort_variable_length_records(infile, outfile, cmp, ram_bytes, n_threads);
    return outfile;
}

string EM_collect_nodes_by_colorset_binary(string infile){
    string outfile = get_temp_file_manager().create_filename();

    seq_io::Buffered_ifstream in(infile, ios::binary);
    seq_io::Buffered_ofstream out(outfile, ios::binary);

    vector<int64_t> active_key;
    vector<int64_t> cur_value_list;

    vector<char> buffer(8);
    vector<int64_t> key; // Reusable space
    while(true){
        key.clear();
        in.read(buffer.data(),8);
        if(in.eof()) break;
        int64_t record_len = parse_big_endian_LL(buffer.data());
        assert(record_len >= 8+8+8);
        while(buffer.size() < record_len)
            buffer.resize(buffer.size()*2);
        in.read(buffer.data()+8,record_len-8); // Read the rest
        int64_t value = parse_big_endian_LL(buffer.data()+8);
        int64_t color_set_size = (record_len - 8 - 8) / 8;
        
        for(int64_t i = 0; i < color_set_size; i++){
            key.push_back(parse_big_endian_LL(buffer.data() + 8 + 8 + i*8));
        }
        if(key == active_key) cur_value_list.push_back(value);
        else{
            if(active_key.size() != 0){
                sort(cur_value_list.begin(), cur_value_list.end());
                int64_t record_size = 8 + 8 + 8*cur_value_list.size() + 8*active_key.size();
                assert(record_size > 0);

                // record = (record length, number of nodes, node list, color list)
                write_big_endian_LL(out, record_size);
                write_big_endian_LL(out, cur_value_list.size());
                for(int64_t x : cur_value_list){
                    write_big_endian_LL(out, x);
                }
                for(int64_t x : active_key){
                    write_big_endian_LL(out, x);
                }
            }
            active_key = key;
            cur_value_list.clear();
            cur_value_list.push_back(value);
        }
    }

    // Last one
    if(active_key.size() != 0){
        sort(cur_value_list.begin(), cur_value_list.end());
        int64_t record_size = 8 + 8 + 8*cur_value_list.size() + 8*active_key.size();
        write_big_endian_LL(out, record_size);
        write_big_endian_LL(out, cur_value_list.size());
        for(int64_t x : cur_value_list){
            write_big_endian_LL(out, x);
        }
        for(int64_t x : active_key){
            write_big_endian_LL(out, x);
        }
    }

    return outfile;
}


