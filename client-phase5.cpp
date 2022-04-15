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
//#include <filesystem>
using namespace std;

// Macros**************************************************************************************************

#define BACKLOG 100     // how many pending connections queue will hold
#define MAXDATASIZE 100 // max number of bytes we can get at once
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
string allns = "";
string curr_dir;
//*********************************************************************************************************************

//****Helper functions***********************************************************************************************

char *to_charS(string s)
{
    char *c = strcpy(new char[s.length() + 1], s.c_str());
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
string getmd5(FILE *inFile)
{
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
    return res;
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
            string s = to_string(this_node.id) + "," + allfiles + "|" + allns;
            char buff[MAX_BUFFER_LEN];
            memset(buff, 0, sizeof(buff));
            char *c3 = to_charS(s);
            strcat(buff, c3);
            const void *info = s.c_str();
            if (send(new_fd, buff, MAX_PACKET_CHUNK_LEN, 0) == -1)
                perror("send");

            for (string filen : this_node.files)
            {
                char *file_name = to_charS(filen);
                char *file_path = to_charS(curr_dir + filen);
                FILE *fp = fopen(file_path, "rb");
                // cerr<<curr_dir+filen<<endl;
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
                char buff[MAX_BUFFER_LEN];
                memset(buff, 0, sizeof(buff));
                int SIZ = GetFilesize(fp);
                char snum[5];
                sprintf(snum, "%d", SIZ);

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
                int sent{};
                while ((R = fread(buff, sizeof(char), MAX_PACKET_CHUNK_LEN, fp)))
                {
                    int x;
                    if ((x = send(new_fd, buff, MAX_PACKET_CHUNK_LEN, 0)) < 0)
                    {
                        perror("Sending Error");
                        exit(1);
                    };
                    sent += x;
                    bzero(buff, MAX_PACKET_CHUNK_LEN);
                }
                // cout<<sent<<endl;
                //  while (!feof(fp))
                //  {
                //      if ((R = fread(buff, sizeof(char), MAX_PACKET_CHUNK_LEN, fp)) > 0)
                //      {
                //          send(new_fd, buff, R, 0);
                //      }
                //      else
                //      {
                //          perror("Error in sending file");
                //          exit(1);
                //      }
                //  }
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
    map<string, vector<pair<int, string>>> m1; // vector of {uniqueID}'s
    map<string, vector<pair<int, string>>> m2; // vector of {uniqueID}'s
    vector<string> added;
    map<int, pair<int, int>> conn;
    set<int> ports1;
    map<string, string> hashs;
    int xx = 0;
    for (int i = 0; i < adj.size(); i++)
    {
        ports1.insert(adj[i].lis_port);
    }
    vector<int> ports2;
    for (int i = 0; i < adj.size(); i++)
    {
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
        int tot{};
        // cout<<"in 1: "<<buffs<<endl;
        ports1.insert(stoi(po));
        conn[adj[i].s_no] = {stoi(po), adj[i].lis_port};
        for (int j = ind + 1; j < buffs.length();)
        {
            if (buffs[j] == ',')
                j++;
            if (buffs[j] == '|')
            {
                ind = j + 1;
                break;
            }
            string filename = "";
            while (buffs[j] != ',')
            {
                filename += buffs[j];
                j++;
            }
            tot++;
            // cout<<"server"<<adj[i].s_no<<" "<<buffs<<endl;
            // int isthere = 0;
            // for (int k = 0; k < req_files.size(); k++)
            // {
            //     if (req_files[k] == filename)
            //     {
            //         isthere = 1;
            //     }
            // }
            // if (isthere)
            // {
            //     m1[filename].push_back(stoi(po));
            // }
        }
        for (int j = ind; j < buffs.length();)
        {
            if (buffs[j] == ',')
                j++;
            if (!(j < buffs.length()))
                break;
            string p1 = "";
            while (buffs[j] != ',')
            {
                p1 += buffs[j];
                j++;
            }
            // cout<<po<<" "<<p1<<endl;
            if ((ports1.find(stoi(p1)) == ports1.end()) && (stoi(p1) != this_node.lis_port))
            {
                ports2.push_back(stoi(p1));
            }
        }
        for (int j = 0; j < tot; j++)
        {
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
            // cout << buffer << endl;
            //  printf("1: %s\n", buffer);
            char *end_pointer;
            char *ch = strtok_r(buffer, "|", &end_pointer);
            strncpy(file_name, ch, strlen(ch));
            strcat(file_name, "\0");
            ch = strtok_r(NULL, " ,", &end_pointer);
            file_data_len = atoi(ch);
            string filename = to_cppString(file_name);
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
            FILE *fp = fopen(file_path, "w");
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

            added.push_back(to_cppString(file_name));
            fclose(fp);
            fp = fopen(file_path, "rb");
            if (downl)
            {
                string md5_hash = getmd5(fp);
                m1[filename].push_back({stoi(po), md5_hash});
            }
            fclose(fp);
        }
        close(sockfd);
    }
    for (int i = 0; i < ports2.size(); i++)
    {
        const char *PORT = to_charS(to_string(ports2[i]));
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
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';
        string buffs = to_cppString(buf);
        string po = ""; // uniqueID;
        int tot{};
        int ind = 0;
        while (buffs[ind] != ',')
        {
            po += buffs[ind];
            ind++;
        }
        // cout<<"in 2: "<<buffs<<endl;
        for (int j = ind + 1; j < buffs.length();)
        {
            if (buffs[j] == ',')
                j++;
            if (buffs[j] == '|')
            {
                ind = j + 1;
                break;
            }
            string filename = "";
            while (buffs[j] != ',')
            {
                filename += buffs[j];
                j++;
            }
            tot++;
            // cout<<"server"<<adj[i].s_no<<" "<<buffs<<endl;
            // int isthere = 0;
            // for (int k = 0; k < req_files.size(); k++)
            // {
            //     if (req_files[k] == filename)
            //     {
            //         isthere = 1;
            //     }
            // }
            // if (isthere)
            // {
            //     m2[filename].push_back(stoi(po));
            // }
        }
        for (int j = 0; j < tot; j++)
        {
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
            //            cout << buffer << endl;

            // printf("1: %s\n", buffer);
            char *end_pointer;
            char *ch = strtok_r(buffer, "|", &end_pointer);
            strncpy(file_name, ch, strlen(ch));
            strcat(file_name, "\0");
            ch = strtok_r(NULL, " ,", &end_pointer);
            file_data_len = atoi(ch);
            string filename = to_cppString(file_name);
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
            FILE *fp = fopen(file_path, "w");
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
            added.push_back(to_cppString(file_name));
            fclose(fp);
            fp = fopen(file_path, "rb");
            if (downl)
            {
                string md5_hash = getmd5(fp);
                m2[filename].push_back({stoi(po), md5_hash});
            }
            fclose(fp);
        }
        close(sockfd);
    }
    for (auto p : conn)
    {
        cout << "Connected to " << p.first << " with unique-ID " << p.second.first << " on port " << p.second.second << endl;
    }
    set<string> strs;
    map<string, pair<int, pair<int, string>>> op; //{uniqueID,depth}
    // for (string s : added)
    // {
    //     int isreq = 0;
    //     for (int j = 0; j < req_files.size(); j++)
    //     {
    //         if (req_files[j] == s)
    //             isreq = 1;
    //     }
    //     if (!isreq)
    //     {
    //         string dir = curr_dir + "Downloaded";
    //         char *file_path = to_charS(dir + "/" + s);
    //         if (unlink(file_path) != 0)
    //         {
    //             cerr << "Error in removing files" << endl;
    //             exit(1);
    //         }
    //     }
    // }
    for (auto X : m1)
    {
        string s1 = X.first;
        strs.insert(s1);
        sort(m1[s1].begin(), m1[s1].end());
        // cout<<"Found "+s1+" at "+m1[s1][0]<<" with MD5 0 at depth 1"<<endl;
        op[s1] = {m1[s1][0].first, {1, m1[s1][0].second}};
    }
    for (auto X : m2)
    {
        string s1 = X.first;
        if (strs.find(s1) != strs.end())
            continue;
        sort(m2[s1].begin(), m2[s1].end());
        // cout<<"Found "+s1+" at "+m2[s1][0]<<" with MD5 0 at depth 2"<<endl;
        // op[s1] = {m2[s1][0], 2};
        op[s1] = {m2[s1][0].first, {2, m2[s1][0].second}};
    }
    for (int i = 0; i < req_files.size(); i++)
    {
        if (op.find(req_files[i]) == op.end())
        {
            op[req_files[i]] = {0, {0, "0"}};
        }
    }
    for (auto p : op)
    {
        string s1 = p.first;
        cout << "Found " + s1 << " at " << p.second.first << " with MD5 " << p.second.second.second << " at depth " << p.second.second.first << endl;
    }
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
    return;
}
//***********************************************************************************

int main(int argc, char *argv[])
{
    string config_file = to_cppString(argv[1]);
    curr_dir = to_cppString(argv[2]);
    fstream myfile;
    myfile.open(config_file, ios::in);
    myfile >> this_node.s_no >> this_node.lis_port >> this_node.id;
    int nc;
    myfile >> nc;
    adj.resize(nc);
    for (int i = 0; i < nc; i++)
    {
        myfile >> adj[i].s_no >> adj[i].lis_port;
        allns += to_string(adj[i].lis_port) + ",";
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