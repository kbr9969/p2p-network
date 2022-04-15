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
#include <sys/stat.h>
#include <openssl/md5.h>
#include <filesystem>

using namespace std;
// Macros**************************************************************************************************

#define BACKLOG 100      // how many pending connections queue will hold
#define MAXDATASIZE 1000 // max number of bytes we can get at once
const int MAX_BUFFER_LEN = 5000;
const int MAX_PACKET_CHUNK_LEN = 1024;
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
string allfiles = "";
string curr_dir;
//*********************************************************************************************************************

//****Helper functions***********************************************************************************************

char *to_charS(string s)
{
    char *c = strcpy(new char[s.length() + 1], s.c_str());
    // cout<<c<<endl;
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

int GetFilesize(FILE *fileid)
{
    fseek(fileid, 0L, SEEK_END);
    int sz = ftell(fileid);
    fseek(fileid, 0L, SEEK_SET);
    return sz;
}

bool IsPathExist(const std::string &s)
{
    struct stat buffer;
    return (stat(s.c_str(), &buffer) == 0);
}
string getmd5(char *file_path)
{
    FILE *inFile = fopen(file_path, "rb");
    unsigned char c[MD5_DIGEST_LENGTH];
    int i;
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    if (inFile == NULL)
    {
        cout << "can't be opened(md5 func).\n";
        exit(1);
    }

    MD5_Init(&mdContext);
    while ((bytes = fread(data, 1, 1024, inFile)) != 0)
        MD5_Update(&mdContext, data, bytes);
    MD5_Final(c, &mdContext);
    string res = "";
    char buf[2 * MD5_DIGEST_LENGTH];
    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(buf, "%02x", c[i]);
        res.append(buf);
    }
    fclose(inFile);
    return res;
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
            string s = to_string(this_node.id) + "," + allfiles;
            char buff[MAX_BUFFER_LEN + 3];
            memset(buff, 0, sizeof(buff));
            char *c3 = to_charS(s);
            strcat(buff, c3);
            const void *info = s.c_str();
            if (send(new_fd, buff, MAX_PACKET_CHUNK_LEN, 0) == -1)
                perror("send");
            // char file_name[MAXDATASIZE];
            // int num{};
            // // cerr << "up" + to_string(this_node.s_no) << endl;
            // while (num == 0)
            // {
            //     if ((num = recv(new_fd, file_name, MAXDATASIZE - 1, 0)) == -1)
            //     {
            //         cout << "server" << this_node.s_no << " ";
            //         perror("recv");
            //         exit(1);
            //     }
            // }
            // file_name[num] = '\0';
            for (string filen : this_node.files)
            {
                char *file_name = to_charS(filen);
                char *file_path = to_charS(curr_dir + to_cppString(file_name));
                FILE *fp = fopen(file_path, "rb");
                // cout<<file_path<<endl;
                if (fp == NULL)
                {
                    // cerr << file_name << endl;
                    cerr << "server" + to_string(this_node.s_no) + to_cppString(file_path) + " File open error" << endl;
                    exit(1);
                }
                if (strlen(file_name) >= MAX_BUFFER_LEN)
                {
                    cerr << "ERROR: Please use a filename less than 256 characters\n";
                    fclose(fp);
                    exit(1);
                }
                char buff[MAX_BUFFER_LEN + 3];
                memset(buff, 0, sizeof(buff));
                int SIZE = GetFilesize(fp);
                char snum[5];
                sprintf(snum, "%d", SIZE);

                // Write all
                memset(buff, 0, sizeof(buff));
                strcat(buff, file_name);
                strcat(buff, "|");
                strcat(buff, snum);
                // printf("FILE_NAME_DATA: %s\n", buff);
                if (strlen(snum) + 1 > MAX_PACKET_CHUNK_LEN)
                {
                    cerr << "Name + Size length exceeded. Error may occur.\n";
                    exit(1);
                }

                send(new_fd, buff, MAX_PACKET_CHUNK_LEN, 0);

                int R;
                memset(buff, 0, sizeof(buff));
                while ((R = fread(buff, sizeof(char), MAX_PACKET_CHUNK_LEN, fp)))
                {
                    if (send(new_fd, buff, MAX_PACKET_CHUNK_LEN, 0) < 0)
                    {
                        perror("Sending Error");
                        exit(1);
                    };
                    bzero(buff, MAX_PACKET_CHUNK_LEN);
                }

                memset(buff, 0, sizeof(buff));
                fclose(fp);
            }
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
    map<string, vector<pair<int, string>>> m; //{uniqueID,md5}
    vector<string> added;
    map<int, pair<int, int>> conn;             //{s_no,{id,port}}
    vector<pair<string, pair<string, int>>> v; //{path,filename}
    int xx = 0;
    for (int i = 0; i < adj.size(); i++)
    {
        // cout<<i<<endl;
        const char *PORT = to_charS(to_string(adj[i].lis_port));
        int sockfd, numbytes;
        char buf[MAX_BUFFER_LEN];
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

        if ((numbytes = recv(sockfd, buf, MAX_PACKET_CHUNK_LEN, 0)) == -1)
        {
            cout << "client" << this_node.s_no << " ";
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';
        string buffs = to_cppString(buf);
        string po = "";
        int ind = 0;
        while (buffs[ind] != ',')
        {
            po += buffs[ind];
            ind++;
        }
        // cout << buffs << endl;
        //  cout<<i<<endl;
        conn[adj[i].s_no] = {stoi(po), adj[i].lis_port};
        int tot{};
        for (int j = ind + 1; j < buffs.length();)
        {
            if (buffs[j] == ',')
            {
                j++;
                continue;
            }
            string filename = "";
            while (buffs[j] != ',')
            {
                filename += buffs[j];
                j++;
            }
            // cout<<filename<<endl;
            tot++;
            // cout<<"server"<<adj[i].s_no<<" "<<buffs<<endl;
        }
        // m[adj[i].s_no] = {stoi(buffs), adj[i].lis_port};
        // cout<<i<<endl;
        for (int j = 0; j < tot; j++)
        {
            // cout<<i<<" "<<j<<endl;
            char buffer[MAX_BUFFER_LEN];
            char file_name[MAX_BUFFER_LEN];
            int file_data_len;

            memset(buffer, 0, sizeof(buffer));
            memset(file_name, 0, sizeof(file_name));

            /* Get File name + len, under 256 characters */
            if (recv(sockfd, buffer, MAX_PACKET_CHUNK_LEN, 0) < 0)
            {
                cerr << "ERROR: Reading file name\n";
                exit(1);
            }
            // printf("1: %s\n", buffer);
            char *end_pointer;
            char *ch = strtok_r(buffer, "|", &end_pointer);
            strncpy(file_name, ch, strlen(ch));
            strcat(file_name, "\0");
            ch = strtok_r(NULL, " ,", &end_pointer);
            string filename = to_cppString(file_name);
            file_data_len = atoi(ch);
            // printf("%sFileName: %s%s\n", KRED, RESET, file_name);
            // printf("%sFilesize: %s%d bytes\n", KRED, RESET, file_data_len);

            /* Create File */
            string dir = curr_dir + "Downloaded";

            if (!IsPathExist(dir))
            {
                int check = mkdir(to_charS(dir), 0777);

                // check if directory is created or not
                if (check)
                {
                    printf("Unable to create directory\n");
                    exit(1);
                }
            }
            char *file_path;
            int downl = 0;
            for (string s2 : req_files)
            {
                if (s2 == filename)
                    downl = 1;
            }
            if (downl)
            {
                file_path = to_charS(dir + "/" + to_cppString(file_name));
            }
            else
            {
                xx = 1;
                file_path = to_charS("rem");
            }
            FILE *fp = fopen(file_path, "wb+");
            // cout<<file_path<<endl;
            if (fp == NULL)
            {
                printf("File open error");
                exit(1);
            }
            int data_received = 0;
            char dd[10];
            int received = 0;
            while (received < file_data_len)
            {
                int R = recv(sockfd, buffer, MAX_PACKET_CHUNK_LEN, 0);
                if (R < 0)
                {
                    cerr << "Error: While receiving\n";
                }
                // if (!fputs(buffer, fp))
                // {
                //     cerr << "ERROR: While saving to file\n";
                // };

                fwrite(&buffer, sizeof(char), min(R, file_data_len - received), fp);
                data_received += R;
                memset(buffer, 0, sizeof(buffer));
                received += R;
            }
            // cout << file_name << " " << received << " " << file_data_len << endl;
            if (received >= file_data_len)
            {
                // printf("%sFile Length:%s nbytes %s%s%s\n", KRED, RESET, KGRN, TICK,
                //      RESET);
                // cout << "Recived >= file_data_len" <<" "<<received<<" "<<file_data_len<< endl;
                // exit(1);
            }
            else
            {
                // printf("%sCONCERN:%s File Length not matching%s\n", KRED, KYEL, RESET);
                //    cout << "Recived < file_data_len" << endl;
                // exit(1);
            }
            added.push_back(to_cppString(file_name));
            v.push_back({to_cppString(file_path), {filename, stoi(po)}});
            fclose(fp);
        }
        // cout<<i<<endl;
        close(sockfd);
    }
    for (auto p : v)
    {
        string filename = p.second.first, fp = p.first;
        int po = p.second.second;
        char *file_path = to_charS(fp);
        FILE *fp1 = fopen(file_path, "rb");
        int isthere = 0;
        for (int k = 0; k < req_files.size(); k++)
        {
            if (req_files[k] == filename)
            {
                isthere = 1;
            }
        }
        if (isthere)
        {
            string md5_hash = getmd5(file_path);
            m[filename].push_back({po, md5_hash});
        }
    }
    for (auto p : conn)
    {
        cout << "Connected to " << p.first << " with unique-ID " << p.second.first << " on port " << p.second.second << endl;
    }
   // cout << xx << endl;
    if (xx)
    {
        char *file_path = to_charS("./rem");
        string ss="rem";
        if (IsPathExist(ss))
        {
            int x = unlink(file_path);
            if (x != 0)
            {
                cerr << file_path << " Error in removing files" << endl;
                exit(1);
            }
        }
        else{
            cerr<<"IsPathExist error"<<endl;
            exit(1);
        }
    }

    for (auto x : m)
    {
        // cout<<"in map "<<x.first<<endl;
        sort(m[x.first].begin(), m[x.first].end());
    }
    for (string s1 : req_files)
    {
        // cout<<"in files "<<s1<<endl;
        if (m[s1].size() != 0)
        {

            cout << "Found " << s1 << " at " << m[s1][0].first << " with MD5 " << m[s1][0].second << " at depth 1" << endl;
        }
        else
        {
            cout << "Found " << s1 << " at 0 with MD5 0 at depth 0" << endl;
        }
    }

    return;
}
//***********************************************************************************

int main(int argc, char *argv[])
{
    string config_file = to_cppString(argv[1]);
    curr_dir = to_cppString(argv[2]);
    // cout<<curr_dir<<endl;
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
    sort(req_files.begin(), req_files.end());
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
                // cout << s << endl;
                allfiles += s;
                allfiles += ",";
                this_node.files.push_back(s);
            }
        }
        sort(this_node.files.begin(), this_node.files.end());
        for (string s1 : this_node.files)
        {
            cout << s1 << endl;
        }
    }
    thread th_server(server);
    thread th_client(client);

    th_server.join();
    th_client.join();

    return 0;
}