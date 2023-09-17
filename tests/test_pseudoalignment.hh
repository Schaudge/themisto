#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include "globals.hh"
#include "sbwt/globals.hh"
#include "sbwt/throwing_streams.hh"
#include "pseudoalign.hh"
#include "zstr.hpp"
#include "test_tools.hh"
#include "setup_tests.hh"
#include <gtest/gtest.h>
#include "include/commands.hh"

using namespace std;
using namespace sbwt;

int64_t random_seed = 123674;

class TestCase{
    public:

    vector<string> genomes;
    unordered_map<string, set<int64_t> > node_to_color_ids; // kmer- > set of color ids
    vector<string> queries;
    vector<string> colex_kmers;
    int64_t n_colors; // Distinct colors
    int64_t k;

    vector<int64_t> seq_to_color_id; // for each sequence: the color of that sequence
};


vector<TestCase> generate_testcases(int64_t genome_length, int64_t n_genomes, int64_t n_queries, int64_t query_length, int64_t min_k, int64_t max_k, int64_t n_colors){
    srand(random_seed);
    vector<TestCase> testcases;

    for(int64_t rep = 0; rep < 5; rep++){
        for(int64_t k = min_k; k <= max_k; k++){
            TestCase tcase;
            tcase.k = k;

            // Build genomes and assign color ids
            for(int64_t i = 0; i < n_genomes; i++){
                tcase.genomes.push_back(get_random_dna_string(genome_length,2));
                tcase.seq_to_color_id.push_back(rand() % n_colors);
            }

            tcase.n_colors = *std::max_element(tcase.seq_to_color_id.begin(), tcase.seq_to_color_id.end()) + 1;

            // Get all k-mers and colex-sort them
            set<string> all_kmers;
            for(int64_t i = 0; i < n_genomes; i++){
                for(string kmer : get_all_distinct_kmers(tcase.genomes[i], k)) all_kmers.insert(kmer);
            }
            
            tcase.colex_kmers = vector<string>(all_kmers.begin(), all_kmers.end());
            sort(tcase.colex_kmers.begin(), tcase.colex_kmers.end(), colex_compare);

            // List k-mer sets for each color
            vector<set<string> > color_to_kmer_set(n_colors);
            for(int64_t genome_id = 0; genome_id < tcase.genomes.size(); genome_id++){
                string genome = tcase.genomes[genome_id];
                int64_t color_id = tcase.seq_to_color_id[genome_id];
                for(string kmer : get_all_distinct_kmers(genome,k)){
                    color_to_kmer_set[color_id].insert(kmer);
                }
            }

            // List all color names for each k-mer (!= each node)
            for(string kmer : tcase.colex_kmers){
                set<int64_t> colorset;
                for(int64_t color_id = 0; color_id < n_colors; color_id++){
                    if(color_to_kmer_set[color_id].count(kmer)){
                        colorset.insert(color_id);
                    }
                }
                tcase.node_to_color_ids[kmer] = colorset;
            }
            

            // Build queries
            for(int64_t i = 0; i < n_queries; i++) tcase.queries.push_back(get_random_dna_string(query_length,2));

            testcases.push_back(tcase);
        }
    }
    return testcases;
}

template <typename T>
set<T> intersect(const set<T>& S1, const set<T>& S2){
    set<T> ans;
    for(auto x : S1){
        if(S2.count(x) > 0){
            ans.insert(x);
        }
    }
    return ans;
}

// Returns set of color names
vector<int64_t> pseudoalign_to_colors_trivial(string& query, TestCase& tcase, bool reverse_complements){
    set<int64_t> alignments;
    for(int64_t i = 0; i < tcase.n_colors; i++) alignments.insert(i); // All color names

    bool at_least_one = false;
    // For each k-mer in query, get the color set and intersect that with alignments
    for(string kmer : get_all_kmers(query, tcase.k)){
        set<int64_t> colorset = tcase.node_to_color_ids[kmer];
        if(reverse_complements){
            set<int64_t> colorset_rc = tcase.node_to_color_ids[sbwt::get_rc(kmer)];
            for(int64_t color : colorset_rc) colorset.insert(color);
        }
        if(colorset.size() >= 1) {
            at_least_one = true;
            alignments = intersect(alignments, colorset);
        }
    }

    if(at_least_one == false) alignments.clear();
    vector<int64_t> ans(alignments.begin(), alignments.end());
    return ans;
}

TEST(TEST_PSEUDOALIGN, coli3_parallelism){
    std::string seqfile = "testcases/coli3.fna";
    std::string colorfile = "testcases/colors.txt";
    std::string queryfile = "example_input/coli_reads.fna";
    std::string indexprefix = get_temp_file_manager().create_filename();
    int64_t k = 31;

    stringstream build_argstring;
    build_argstring << "build -k"  << k << " --n-threads " << 2 << " --mem-megas " << 2048 << " -i " << seqfile << " -c " << colorfile << " --colorset-pointer-tradeoff 3 " << " -o " << indexprefix << " --temp-dir " << sbwt::get_temp_file_manager().get_dir() << " --forward-strand-only";
    Argv build_argv(split(build_argstring.str()));

    ASSERT_EQ(build_index_main(build_argv.size, build_argv.array),0);

    // Run with with parallelism
    string parallel_resultfile = get_temp_file_manager().create_filename();
    stringstream pseudoalign_parallel_argstring;
    pseudoalign_parallel_argstring << "pseudoalign --rc -q " << queryfile << " -i " << indexprefix << " -o " << parallel_resultfile << " --n-threads " << 128 << " --temp-dir " << get_temp_file_manager().get_dir()  << " --buffer-size-megas 0.00001 --sort-output"; // 128 threads and a really small buffer to expose race conditions
    Argv pseudoalign_parallel_argv(split(pseudoalign_parallel_argstring.str()));
    ASSERT_EQ(pseudoalign_main(pseudoalign_parallel_argv.size, pseudoalign_parallel_argv.array),0);

    // Run without parallelism
    string sequential_resultfile = get_temp_file_manager().create_filename();
    stringstream pseudoalign_sequential_argstring;
    pseudoalign_sequential_argstring << "pseudoalign --rc -q " << queryfile << " -i " << indexprefix << " -o " << sequential_resultfile << " --n-threads " << 1 << " --temp-dir " << get_temp_file_manager().get_dir()  << " --buffer-size-megas 0.00001"; // 1 thread
    Argv pseudoalign_sequential_argv(split(pseudoalign_sequential_argstring.str()));
    ASSERT_EQ(pseudoalign_main(pseudoalign_sequential_argv.size, pseudoalign_sequential_argv.array),0);

    ASSERT_TRUE(files_are_equal(parallel_resultfile, sequential_resultfile));

}

TEST(TEST_PSEUDOALIGN, intersection_random_testcases){
    logger << "Testing pseudolign" << endl;

    int64_t testcase_id = -1;
    int64_t ref_length = 100;
    int64_t n_refs = 50;
    int64_t n_queries = 10000;
    int64_t query_length = 20;
    int64_t k_min = 1;
    int64_t k_max = 20;
    int64_t n_colors = 5;
    for(TestCase tcase : generate_testcases(ref_length, n_refs, n_queries, query_length, k_min,k_max, n_colors)){
        testcase_id++;
        logger << "Running alignment testcase" << endl;

        string genomes_outfilename = sbwt::get_temp_file_manager().create_filename("genomes-",".fna");
        string queries_outfilename = sbwt::get_temp_file_manager().create_filename("queries-",".fna");
        string queries_gzip_outfilename = sbwt::get_temp_file_manager().create_filename("queries-",".fna.gz");
        string colorfile_outfilename = sbwt::get_temp_file_manager().create_filename("colorfile-",".txt");
        string index_prefix = sbwt::get_temp_file_manager().get_dir() + "/test_index";

        sbwt::throwing_ofstream genomes_out(genomes_outfilename);
        for(string genome : tcase.genomes){
            genomes_out << ">\n" << genome << "\n";
        }
        genomes_out.close();

        sbwt::throwing_ofstream colors_out(colorfile_outfilename);
        for(int64_t i = 0; i < tcase.seq_to_color_id.size(); i++){
            colors_out << tcase.seq_to_color_id[i] << "\n";
        }
        colors_out.close();

        sbwt::throwing_ofstream queries_out(queries_outfilename);
        
        seq_io::zstr::ofstream* queries_out_gzip = new seq_io::zstr::ofstream(queries_gzip_outfilename);
        for(string query : tcase.queries){
            queries_out << ">\n" << query << "\n";
            *queries_out_gzip << ">\n" << query << "\n";
        }
        queries_out.close();
        delete queries_out_gzip; // Flushes the stream

        stringstream build_argstring;
        build_argstring << "build -k "  << tcase.k << " --n-threads " << 2 << " --mem-megas " << 2048 << " -i " << genomes_outfilename << " -c " << colorfile_outfilename << " --colorset-pointer-tradeoff 3 " << " -o " << index_prefix << " --temp-dir " << sbwt::get_temp_file_manager().get_dir() << " --forward-strand-only";;
        Argv build_argv(split(build_argstring.str()));

        ASSERT_EQ(build_index_main(build_argv.size, build_argv.array),0);

        // Run without rc
        string final_file = sbwt::get_temp_file_manager().create_filename("finalfile-");
        stringstream pseudoalign_argstring;
        pseudoalign_argstring << "pseudoalign -q " << queries_outfilename << " -i " << index_prefix << " -o " << final_file << " --n-threads " << 3 << " --temp-dir " << sbwt::get_temp_file_manager().get_dir() << " buffer-size-megas 0.00001 --threshold 1"; // Really small buffer to expose race conditions
        Argv pseudoalign_argv(split(pseudoalign_argstring.str()));

        ASSERT_EQ(pseudoalign_main(pseudoalign_argv.size, pseudoalign_argv.array),0);

        vector<vector<int64_t> > our_results = parse_pseudoalignment_output_format_from_disk(final_file);

        // Run with rc
        string final_file_rc = get_temp_file_manager().create_filename("finalfile_rc-");
        stringstream pseudoalign_rc_argstring;
        pseudoalign_rc_argstring << "pseudoalign --rc -q " << queries_outfilename << " -i " << index_prefix << " -o " << final_file_rc << " --n-threads " << 3 << " --temp-dir " << get_temp_file_manager().get_dir()  << " buffer-size-megas 0.00001 --sort-output --threshold 1"; // Really small buffer to expose race conditions
        Argv pseudoalign_rc_argv(split(pseudoalign_rc_argstring.str()));
        ASSERT_EQ(pseudoalign_main(pseudoalign_rc_argv.size, pseudoalign_rc_argv.array),0);

        vector<vector<int64_t> > our_results_rc = parse_pseudoalignment_output_format_from_disk(final_file_rc);

        // Run with threshold method with threshold 1.0 ignoring unknown k-mers. Should be the same as intersection with rc
        string final_file_rc_threshold_ignore_unknown = get_temp_file_manager().create_filename("final_file_rc_threshold_ignore_unknown-");
        stringstream pseudoalign_rc_threshold_ignore_unknown_argstring;
        pseudoalign_rc_threshold_ignore_unknown_argstring << "pseudoalign --threshold 1 --ignore-unknown-kmers --rc -q " << queries_outfilename << " -i " << index_prefix << " -o " << final_file_rc_threshold_ignore_unknown << " --n-threads " << 3 << " --temp-dir " << get_temp_file_manager().get_dir()  << " buffer-size-megas 0.00001 --sort-output"; // Really small buffer to expose race conditions
        Argv pseudoalign_rc_threshold_ignore_unknown_argv(split(pseudoalign_rc_threshold_ignore_unknown_argstring.str()));
        ASSERT_EQ(pseudoalign_main(pseudoalign_rc_threshold_ignore_unknown_argv.size, pseudoalign_rc_threshold_ignore_unknown_argv.array),0);

        vector<vector<int64_t> > our_rc_threshold_ignore_unknown = parse_pseudoalignment_output_format_from_disk(final_file_rc_threshold_ignore_unknown);

        // Run with gzipped input
        string final_file_gzip = get_temp_file_manager().create_filename("finalfile_gzip-");
        stringstream pseudoalign_gzip_argstring;
        pseudoalign_gzip_argstring << "pseudoalign -q " << queries_gzip_outfilename << " -i " << index_prefix << " -o " << final_file_gzip << " --n-threads " << 3 << " --temp-dir " << get_temp_file_manager().get_dir() << " --sort-output --threshold 1";
        Argv pseudoalign_gzip_argv(split(pseudoalign_gzip_argstring.str()));
        ASSERT_EQ(pseudoalign_main(pseudoalign_gzip_argv.size, pseudoalign_gzip_argv.array),0);

        vector<vector<int64_t> > our_results_gzip = parse_pseudoalignment_output_format_from_disk(final_file_gzip);

        for(int64_t i = 0; i < tcase.queries.size(); i++){
            string query = tcase.queries[i];

            vector<int64_t> brute = pseudoalign_to_colors_trivial(query, tcase, false);
            vector<int64_t> brute_rc = pseudoalign_to_colors_trivial(query, tcase, true);

            //logger << brute << endl << brute_rc << "-" << endl;

            ASSERT_EQ(brute, our_results[i]);
            ASSERT_EQ(brute, our_results_gzip[i]);
            ASSERT_EQ(brute_rc, our_results_rc[i]);
            ASSERT_EQ(brute_rc, our_rc_threshold_ignore_unknown[i]);
        }
    }
}

TEST(TEST_PSEUDOALIGN, thresholded){
    vector<string> seqs = {"ACATGACGACACATGCTGTAC", // Random keyboard mashing
                           "AACTATGGTGCTAACGTAGCAC", // Random keyboard mashing
                           "GTGTAGTAGTGTGTAGTAGCATGGGCAC", // Random keyboard mashing
                           "GTGTAGTAGTGTGTTGTAGCATGGGCAC", // Copy of previous with one mutation in the middle
                           "GTGCCCATGCTACTACACACTACTACAC", // RC of seqs[3]
                           "GTGCCCATGCTACAACACACTACTACAC"}; // RC of seqs[4]

    int64_t k = 6;
    vector<string> queries = {"ACATGACGACACATGCTGTAC", // Exact match to seq 0
                              "GTACAGCATGTGTCGTCATGT", // Reverse complement of seq 0
                              "AACTATGGTGCTAACGTAGCAC", // Exact match to seq 1
                              "GTGCTACGTTAGCACCATAGTT", // Reverse complement of seq 1
                              "ACATGACGATACATGCTGTAC", // Single mutation to seq 0
                              "GTACAGCATTTGTCGTCATGT", // Single mutation to RC of seq 0
                              "AACTATGGTTCTAACGTAGCAC", // Single mutation to seq 1
                              "GTGCTACGTAAGCACCATAGTT", // Single mutation to RC of seq 1
                              "GTGTAGTAGTGTGTAGTAGCATGGGCAC", // Exact match to seq 2
                              "GTGTAGTAGTGTGTTGTAGCATGGGCAC", // Exact match to seq 3
                              "GTGCCCATGCTACTACACACTACTACAC", // Exact match to seq 4
                              "GTGCCCATGCTACAACACACTACTACAC", // Exact match to seq 5
                              "AC"}; // Shorter than k

    // Append stuff to seqs[5] to check that the query turns
    // negative at the right time when not enough k-mers are found
    for(int64_t len = 1; len <= 40; len++){
        string suffix(len, 'A');
        queries.push_back(seqs[5] + suffix);
    }

    double threshold = 0.5;

    vector<vector<int64_t> > true_answers;
    for(string Q : queries){
        vector<int64_t> counters(seqs.size());
        for(string x : get_all_kmers(Q,k)){
            for(int64_t color = 0; color < seqs.size(); color++){
                bool found = 0;
                if(seqs[color].find(x) != string::npos) found = true;
                if(get_rc(seqs[color]).find(x) != string::npos) found = true;

                counters[color] += found; // k-mer `x` is found in color `color`
            }
        }

        vector<int64_t> answer;
        for(int64_t color = 0; color < seqs.size(); color++){
            if(Q.size() >= k && counters[color] >= ((int64_t)Q.size()-k+1) * threshold){
                answer.push_back(color);
            }
        }
        true_answers.push_back(answer);
    }

    string ref_fastafile = get_temp_file_manager().create_filename("", ".fna");
    string query_fastafile = get_temp_file_manager().create_filename("", ".fna");
    string resultfile = get_temp_file_manager().create_filename("", ".txt");
    string indexprefix = get_temp_file_manager().create_filename();
    string tempdir = get_temp_file_manager().get_dir();
    write_as_fasta(seqs, ref_fastafile);
    write_as_fasta(queries, query_fastafile);

    vector<string> args = {"build", "-k", to_string(k), "-i", ref_fastafile, "-o", indexprefix, "--temp-dir", tempdir, "--forward-strand-only"};
    sbwt::Argv argv(args);
    build_index_main(argv.size, argv.array);
    plain_matrix_sbwt_t SBWT; Coloring<> coloring;
    SBWT.load(indexprefix + ".tdbg");
    coloring.load(indexprefix + ".tcolors", SBWT);

    vector<string> args2 = {"pseudoalign", "-q", query_fastafile, "-i", indexprefix, "-o", resultfile, "--temp-dir", tempdir, "--rc", "--threshold", to_string(threshold), "--sort-output", "--include-unknown-kmers"};
    sbwt::Argv argv2(args2);
    pseudoalign_main(argv2.size, argv2.array);

    vector<vector<int64_t> > results = parse_pseudoalignment_output_format_from_disk(resultfile);
    
    ASSERT_EQ(results.size(), queries.size());
    for(int64_t i = 0; i < results.size(); i++){
        std::sort(results[i].begin(), results[i].end());
        logger << queries[i] << endl;
        print(results[i], logger);
        print(true_answers[i], logger);
        logger << "==" << endl;
        ASSERT_EQ(results[i], true_answers[i]);
    }

    //void pseudoalign_thresholded(const plain_matrix_sbwt_t& SBWT, const coloring_t& coloring, int64_t n_threads, sequence_reader_t& reader, std::string outfile, bool reverse_complements, int64_t buffer_size, bool gzipped, bool sorted_output)
}
