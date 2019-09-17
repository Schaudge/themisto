#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iterator>
#include <cmath>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <map>
#include "TempFileManager.hh"
#include <signal.h>
#include "input_reading.hh"

using namespace std;

typedef int64_t LL;
const char read_separator = '$';

map<string,vector<string> > parse_args(int argc, char** argv){
    // Options are argumenta that start with "--". All non-options
    // that come after an option are parameters for that option
    map<string,vector<string> > M; // Option -> list of parameters
    string current_option = "";
    for(LL i = 1; i < argc; i++){
        string S = argv[i];
        if(S.size() >= 2  && S.substr(0,2) == "--"){
            current_option = S;
            M[current_option].resize(0); // Add empty vector for this option.
        } else{
            if(current_option == ""){
                cerr << "Error parsing command line parameters" << endl;
                exit(1);
            }
            M[current_option].push_back(S);
        }
    }
    return M;
}

Temp_File_Manager temp_file_manager;

void sigint_handler(int sig) {
    cerr << "caught signal: " << sig << endl;
    cerr << "Cleaning up temporary files" << endl;
    temp_file_manager.clean_up();
    exit(1);
}

void sigabrt_handler(int sig) {
    cerr << "caught signal: " << sig << endl;
    cerr << "Cleaning up temporary files" << endl;
    temp_file_manager.clean_up();
    cerr << "Aborting" << endl;
    exit(1);
}

auto sigint_register_return_value = signal(SIGINT, sigint_handler); // Set the SIGINT handler
auto sigabrt_register_return_value = signal(SIGABRT, sigabrt_handler); // Set the SIGABRT handler

void set_temp_dir(string temp_dir){
    temp_file_manager.set_dir(temp_dir);
}

string get_temp_file_name(string prefix){
    return temp_file_manager.get_temp_file_name(prefix);
}

vector<string> get_first_and_last_kmers(string fastafile, LL k){
    // todo: this is pretty expensive because this has to read the whole reference data
    FASTA_reader fr(fastafile);
    vector<string> result;
    while(!fr.done()){
        string ref = fr.get_next_query_stream().get_all();
        if(ref.size() >= k){
            result.push_back(ref.substr(0,k));
            result.push_back(ref.substr(ref.size()-k,k));
        }
    }
    return result;
}



// true if S is colexicographically-smaller than T
bool colex_compare(const string& S, const string& T){
    LL i = 0;
    while(true){
        if(i == S.size() || i == T.size()){
            // One of the strings is a suffix of the other. Return the shorter.
            if(S.size() < T.size()) return true;
            else return false;
        }
        if(S[S.size()-1-i] < T[T.size()-1-i]) return true;
        if(S[S.size()-1-i] > T[T.size()-1-i]) return false;
        i++;
    }
}

template <typename T>
vector<T> parse_tokens(string S){
    vector<T> tokens;
    stringstream ss(S);
    T token;
    while(ss >> token) tokens.push_back(token);
    
    return tokens;
}

vector<string> split(string text){
    std::istringstream iss(text);
    std::vector<std::string> results(std::istream_iterator<std::string>{iss},
                                 std::istream_iterator<std::string>());
    return results;
}

string getTimeString(){
    std::time_t result = std::time(NULL);
    string time = std::asctime(std::localtime(&result));
    return time.substr(0,time.size() - 1); // Trim the trailing newline
}

static bool logging_enabled = true;

void enable_logging(){
    logging_enabled = true;
}

void disable_logging(){
    logging_enabled = false;
}

void write_log(string message){
    if(logging_enabled){
        std::cerr << getTimeString() << " " << message << std::endl;
    }
}

// https://stackoverflow.com/questions/18100097/portable-way-to-check-if-directory-exists-windows-linux-c
void check_dir_exists(string path){
    struct stat info;    
    if( stat( path.c_str(), &info ) != 0 ){
        cerr << "Error: can not access directory " << path << endl;
        exit(1);
    }
    else if( info.st_mode & S_IFDIR ){
        // All good
    }    
    else{
        cerr << "Error: is not a directory: " << path << endl;
        exit(1);
    }
}

void check_readable(string filename){
    ifstream F(filename);
    if(!F.good()){
        cerr << "Error reading file: " << filename << endl;
        exit(1);
    }
}

// Also clears the file
void check_writable(string filename){
    ofstream F(filename, std::ofstream::out | std::ofstream::app);
    if(!F.good()){
        cerr << "Error writing to file: " << filename << endl;
        exit(1);
    }
}


template <typename T>
void write_to_file(string path, T& thing){
    ofstream out(path);
    if(!out.good()){
        cerr << "Error writing to " << path << endl;
        exit(1);
    }
    out << thing << endl;
}


template <typename T>
void read_from_file(string path, T& thing){
    ifstream input(path);
    if(!input.good()){
        cerr << "Error reading file: " << path << endl;
        exit(1);
    } else{
        input >> thing;
    }    
}

class Progress_printer{

    public:

    LL n_jobs;
    LL processed;
    LL total_prints;
    LL next_print;

    Progress_printer(LL n_jobs, LL total_prints) : n_jobs(n_jobs), processed(0), total_prints(total_prints), next_print(0) {}

    void job_done(){
        if(next_print == processed){
            LL progress_percent = round(100 * ((double)processed / n_jobs));
            write_log("Progress: " + to_string(progress_percent) + "%");
            next_print += n_jobs / total_prints;
        }
        processed++;
    }

};
