#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <thread>
#include <sys/wait.h>


using namespace std;
// Macros**************************************************************************************************

#define BACKLOG 100     // how many pending connections queue will hold
#define MAXDATASIZE 100 // max number of bytes we can get at once

// ******************************************************************************************************************

// Global variables**************************************************************************************************

struct node
{
    int s_no;
    int id;
    int lis_port;
    vector<string> files;
};
vector<string> req_files;
vector<node> adj;
node this_node;
string allfiles="";
//*********************************************************************************************************************

//****Helper functions***********************************************************************************************

char *to_charS(string s)
{
    char *c = const_cast<char *>(s.c_str());
    return c;
}

string to_cppString(char *c)
{
    string s = "";
    for (int i = 0;; i++)
    {
        if (c[i] == '\0')
            break;
        s += c[i];
    }
    return s;
}
//*********************************************************************************************************************
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

//******SERVER*****************************************************************************************************
void server()
{
    const char *PORT = to_charS(to_string(this_node.lis_port));
    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    while (1)
    { // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
       // printf("server: got connection from %s\n", s);

        if (!fork())
        {                  // this is the child process
            close(sockfd); // child doesn't need the listener
            string s = to_string(this_node.id)+","+allfiles;
            const void *info = s.c_str();
            if (send(new_fd, info, s.length(), 0) == -1)
                perror("send");
            close(new_fd);
            exit(0);
        }
        close(new_fd); // parent doesn't need this
    }
}
//**********************************************************************************************

//*******CLIENT********************************************************************************
void client()
{
    map<string,vector<pair<pair<int,int>,int>>> m; //{{uniqueID,s_no},port}
    for (int i = 0; i < adj.size(); i++)
    {
        const char *PORT = to_charS(to_string(adj[i].lis_port));
        int sockfd, numbytes;
        char buf[MAXDATASIZE];
        struct addrinfo hints, *servinfo, *p = NULL;
        int rv;
        char s[INET6_ADDRSTRLEN];

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // use my IP
        while (p == NULL)
        {
            if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
            {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                exit(1);
            }

            // loop through all the results and connect to the first we can
            for (p = servinfo; p != NULL; p = p->ai_next)
            {
                if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                     p->ai_protocol)) == -1)
                {
                    // perror("client: socket");
                    continue;
                }

                if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
                {
                    close(sockfd);
                    // perror("client: connect");
                    continue;
                }

                break;
            }

            // if (p == NULL)
            // {
            //     fprintf(stderr, "client: failed to connect\n");
            //     exit(2);
            // }
        }
        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                  s, sizeof s);
       // printf("client: connecting to %s\n", s);

        freeaddrinfo(servinfo); // all done with this structure

        if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
        {
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';
        string buffs = to_cppString(buf);
        string po="";
        int ind=0;
        while(buffs[ind]!=','){
            po+=buffs[ind];
            ind++;
        }
        for(int j=ind+1;j<buffs.length();){
            if(buffs[j]==',') {
                j++;
            }
            if(!(j<buffs.length())) break;
            string filename="";
            while(buffs[j]!=','){
                filename+=buffs[j];
                j++;
            }
           // cout<<"server"<<adj[i].s_no<<" "<<buffs<<endl;
            int isthere=0;
            for(int k=0;k<req_files.size();k++){
                if(req_files[k]==filename){
                    isthere=1;
                }
            }
            if(isthere){
                m[filename].push_back({{stoi(po),i},adj[i].lis_port});
            }
        }
        //m[adj[i].s_no] = {stoi(buffs), adj[i].lis_port};

        close(sockfd);
    }
    for(auto x:m){
        //cout<<"in map "<<x.first<<endl;
        sort(m[x.first].begin(),m[x.first].end());
    }
    for(string s1:req_files){
        //cout<<"in files "<<s1<<endl;
        if(m[s1].size()!=0){
            cout<<"Found "<<s1<<" at "<<m[s1][0].first.first<<" with MD5 0 at depth 1"<<endl;
        }
        else{
            cout<<"Found "<<s1<<" at 0 with MD5 0 at depth 0"<<endl;
        }
    }
    return;
}
//***********************************************************************************

int main(int argc, char *argv[])
{
    string config_file = to_cppString(argv[1]);
    string curr_dir = to_cppString(argv[2]);
    fstream myfile;
    myfile.open(config_file, ios::in);
    myfile >> this_node.s_no >> this_node.lis_port >> this_node.id;
    int nc;
    myfile >> nc;
    adj.resize(nc);
    for (int i = 0; i < nc; i++)
    {
        myfile >> adj[i].s_no >> adj[i].lis_port;
    }
    int nf;
    myfile >> nf;
    req_files.resize(nf);
    for (int i = 0; i < nf; i++)
    {
        myfile >> req_files[i];
    }

    // pringting files
    DIR *dir;
    struct dirent *file;
    if ((dir = opendir(argv[2])) != nullptr)
    {
        while ((file = readdir(dir)) != nullptr)
        {
            if (file->d_type == DT_REG)
            {
                string s = file->d_name;
                //cout << s << endl;
                allfiles+=s;
                allfiles+=",";
                this_node.files.push_back(s);
            }
        }
    }
    thread th_server(server);
    thread th_client(client);

    th_server.join();
    th_client.join();

    return 0;
}