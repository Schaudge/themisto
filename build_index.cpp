#include "Themisto.hh"
#include "globals.hh"
#include "zpipe.hh"
#include <string>
#include <cstring>

using namespace std;

struct Config{
    LL k = -1;
    LL n_threads = 1;
    string inputfile;
    string colorfile;
    string index_dir;
    string temp_dir;
    string input_format;
    bool load_boss = false;
    LL memory_megas = 1000;
    bool auto_colors = false;

    void check_valid(){
        check_readable(inputfile);
        assert(input_format != "");

        if(!load_boss){
            assert(k != -1);
        }

        if(colorfile != ""){
            check_readable(colorfile);
        }
        
        assert(index_dir != "");
        check_dir_exists(index_dir);

        assert(temp_dir != "");
        check_dir_exists(temp_dir);
    }

    string to_string(){
        stringstream ss;
        ss << "Input file = " << inputfile << "\n";
        ss << "Input format = " << input_format << "\n";
        if(colorfile != "") ss << "Color name file = " << colorfile << "\n";
        ss << "Index directory = " << index_dir << "\n";
        ss << "Temporary directory = " << temp_dir << "\n";
        ss << "k = " << k << "\n";
        ss << "Number of threads = " << n_threads << "\n";
        ss << "Memory megabytes = " << memory_megas << "\n";
        ss << "Automatic colors = " << (auto_colors ? "true" : "false") << "\n";
        ss << "Load BOSS = " << (load_boss ? "true" : "false"); // Last has no endline
        return ss.str();
    }
};

int main2(int argc, char** argv){
    Themisto themisto;
    if(argc == 1){
        cerr << "Options: " << endl;
        cerr << "  --load-boss (if given, loads a precomputed boss from the index directory)" << endl;
        cerr << "  --k [value of k] (required only if --load-boss is not given)" << endl;
        cerr << "  --input-file [filename] (The input sequences in FASTA or FASTQ format. The format" << endl;
        cerr << "                           is inferred from the file extension. Recognized file extensions for" << endl;
        cerr << "                           fasta are: .fasta, .fna, .ffn, .faa and .frn . Recognized extensions for" << endl;
        cerr << "                           fastq are: .fastq and .fq . If the file ends with .gz, it is uncompressed" << endl;
        cerr << "                           into a temporary directory and the temporary file is deleted after use." << endl;
        cerr << "  --color-file [filename] (one color per sequence in the fasta file, one color name per line." << endl;
        cerr << "                          Required only if you want to build the colors)" << endl;
        cerr << "  --auto-colors (instead of a color file let the program automatically give colors integer names (0,1,2...))" << endl;
        cerr << "  --index-dir [path] (Directory where the index will be built. Always required, directory must" << endl;
        cerr << "                      exist before running)" << endl;
        cerr << "  --temp-dir [path] (Temporary direction. Always required, directory must exist before running)" << endl;
        cerr << "  --mem-megas [number] (Number of megabytes allowed for external memory algorithms. Default: 1000)" << endl;
        cerr << "  --n-threads [number] (number of parallel threads to use. Default: 1)" << endl;
        cerr << "Usage examples:" << endl;
        cerr << "Build BOSS and colors:" << endl;
        cerr << "  ./build_index --k 31 --mem-megas 10000 --input-file references.fna --color-file colors.txt --index-dir index --temp-dir temp" << endl;
        cerr << "Build only the BOSS" << endl;
        cerr << "  ./build_index --k 31 --mem-megas 10000 --input-file references.fna --index-dir index --temp-dir temp" << endl;
        cerr << "Load a previously built BOSS from the index directory and compute the colors:" << endl;
        cerr << "  ./build_index --mem-megas 10000 --input-file references.fna --color-file colors.txt --index-dir index --temp-dir temp --load-boss" << endl;
        exit(1);
    }

    Config C;
    for(auto keyvalue : parse_args(argc, argv)){
        string option = keyvalue.first;
        vector<string> values = keyvalue.second;
        if(option == "--k"){
            assert(values.size() == 1);
            C.k = std::stoll(values[0]);
        } else if(option == "--input-file"){
            assert(values.size() == 1);
            assert(C.inputfile == "");
            C.inputfile = values[0];
            C.input_format = figure_out_file_format(values[0]);
        } else if(option == "--n-threads"){
            assert(values.size() == 1);
            C.n_threads = std::stoll(values[0]);
        } else if(option == "--color-file"){
            assert(values.size() == 1);
            C.colorfile = values[0];
        } else if(option == "--index-dir"){
            assert(values.size() == 1);
            C.index_dir = values[0];
        } else if(option == "--temp-dir"){
            assert(values.size() == 1);
            C.temp_dir = values[0];
        } else if(option == "--load-boss"){
            assert(values.size() == 0);
            C.load_boss = true;
        } else if(option == "--mem-megas"){
            assert(values.size() == 1);
            C.memory_megas = std::stoll(values[0]);
        } else if(option == "--auto-colors"){
            assert(values.size() == 0);
            C.auto_colors = true;
        } else{
            cerr << "Error parsing command line arguments. Unkown option: " << option << endl;
            exit(1);
        }
    }

    C.check_valid();
    temp_file_manager.set_dir(C.temp_dir);

    cerr << C.to_string() << endl;
    write_log("Starting");

    if(C.input_format == "gzip"){
        write_log("Decompressing the input file");
        string new_name = temp_file_manager.get_temp_file_name("input");
        assert(gz_decompress(C.inputfile, new_name) == Z_OK);
        C.input_format = figure_out_file_format(C.inputfile.substr(0,C.inputfile.size() - 3));
        C.inputfile = new_name;
    }

    Sequence_Reader sr(C.inputfile, C.input_format == "fasta" ? FASTA_MODE : FASTQ_MODE);
    C.inputfile = fix_alphabet(sr); // Turns the file into fasta format also
    C.input_format = "fasta";

    if(C.load_boss){
        write_log("Loading BOSS");
        themisto.load_boss(C.index_dir + "/boss-");
    } else{
        write_log("Building BOSS");
        themisto.construct_boss(C.inputfile, C.k, C.memory_megas * 1e6, C.n_threads);
        themisto.save_boss(C.index_dir + "/boss-");
        write_log("Building BOSS finished (" + std::to_string(themisto.boss.get_number_of_nodes()) + " nodes)");
    }

    if(C.colorfile != "" || C.auto_colors){
        write_log("Building colors");
        themisto.construct_colors(C.inputfile, C.auto_colors ? "" : C.colorfile, C.memory_megas * 1e6, C.n_threads);
        themisto.save_colors(C.index_dir + "/coloring-");
    }

    write_log("Finished");

    return 0;
}

int main(int argc, char** argv){
    try{
        return main2(argc, argv);
    } catch (const std::runtime_error &e){
        std::cerr << "Runtime error: " << e.what() << '\n';
        return 1;
    }
}