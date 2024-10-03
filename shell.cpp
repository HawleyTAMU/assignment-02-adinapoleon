#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>
#include <sys/stat.h>
#include <fcntl.h>
#include <chrono>

#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {

    //get current working directory
    auto currPath = filesystem::current_path();
    string prevDir = currPath;
    string currDir = currPath;

    //saving the original stdin and stdout descriptors
    int OG_read = dup(0);
    int OG_write = dup(1);

    //vector to keep track of processes
    vector<pid_t> processes;

    for (;;) {

        //checking the status of processes
        for (int i = (int)processes.size() - 1; i >= 0; i--) {
            int status;
            waitpid(processes[i], &status, WNOHANG);
        }

        //getting the current time and formating it
        auto tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
        string currTime = ctime(&tm);
        currTime.erase(currTime.length() - 1); //erase newline

        //getting the current directory path as a string
        string path = std::filesystem::current_path();

        // need date/time, username, and absolute path to current dir
        cout << YELLOW << "Shell$" << BLUE << " ~" << getenv("USER") << "~ " << GREEN << currTime << " " << WHITE << path << NC << " ";

        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "") { //if no input, continue the next prompt
            continue;
        }

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

        //if "cd", change directory
        if (tknr.commands[0]->args[0] == "cd") {

            //go to prev directory
            if (tknr.commands[0]->args[1] == "-") {
                cout << "Going to Previous Location" << endl;
                chdir(prevDir.c_str());
                prevDir = currDir;
                currDir = std::filesystem::current_path();
                continue;
            } else { // change to specific directory
                cout << "Going up a directory" << endl;
                chdir(tknr.commands[0]->args[1].c_str());
                prevDir = currDir;
                currDir = std::filesystem::current_path(); 
                continue;
            }

        }

        pid_t pid;

        for (unsigned long int i = 0; i < tknr.commands.size(); i++) {

            // Create pipe
            int fd[2];
            pipe(fd);
            pid = fork(); // forking the child process

            if (pid == 0) { //child process

                if (i < tknr.commands.size() - 1) {  //if not last command                                   
                    dup2(fd[1], STDOUT_FILENO); // redirect output of execvp to the write end of the pipe
                }

                //if command has an output 
                if (tknr.commands[i]->hasOutput()) {
                    // removing semicolon from the back of an out_file
                    string temp = tknr.commands[i]->out_file;
                    if (temp[temp.size() - 1] == ';') {
                        temp = temp.substr(0, temp.size() - 1);
                    }

                    int fd = open((temp.c_str()), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                if (tknr.commands[i]->hasInput()) {
                    int fd = open((tknr.commands[i]->in_file.c_str()), O_RDONLY, S_IRUSR | S_IWUSR);
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                close(fd[0]);
                vector<char *> commands;

                for (auto const &a : tknr.commands[i]->args) {
                    commands.push_back(const_cast<char *>(a.c_str()));
                }
                commands.push_back(NULL);

                if (execvp(commands[0], commands.data()) < 0) {// overwrites entirety of below file
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            }

            else if (pid > 0) {
                // redirect SHELL(parent) input to read end of pipe
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]);


                if (!(tknr.commands[i]->isBackground())) {    
                    waitpid(pid, NULL, 0); // wait until each child process finishes in order
                } else {
                    processes.push_back(pid);
                }

            }

            close(fd[0]);
            close(fd[1]);
        }

        // reset both input and output after messing around w/ redirecting to READ and WRITE ends of the pipe
        dup2(OG_read, STDIN_FILENO);
        dup2(OG_write, STDOUT_FILENO);
    }

    dup2(STDIN_FILENO, OG_read);   // restoring stdin
    dup2(STDOUT_FILENO, OG_write); // restoring stdout

}
