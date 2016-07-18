#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sqlite3.h>

//mongodb c api头文件
#include <libbson-1.0/bson.h>
#include <libbson-1.0/bcon.h>
#include <libmongoc-1.0/mongoc.h>

//定义一个结构体用于保存mongodb连接参数。
typedef struct mongoc_struct{
    mongoc_client_t      *client;
    mongoc_database_t    *database;
    mongoc_collection_t  *collection;
    bson_t               *command,
                         reply,
                         *insert;
    bson_error_t          error;
    bool                  retval;
}MC;

int chk_table_exists_callback(void *arg,int nr,char **values,char **names)
{
    static char *isExist;
    int i;

    isExist = (char *)arg;
    for(i=0;i<nr;i++){
        sprintf(isExist,"%s",values[i]);
    }
    return(0);
}

int create_db_objects(void){
    static char *isTableExist;
    isTableExist = (char *)malloc(sizeof(char)*16);

    char *query = NULL;

    sqlite3 *db;
    sqlite3_open("/etc/sysconfig/pdb/rewind.db",&db);

    query = (char *)malloc(sizeof(char)*4096);
    memset(query,0,sizeof(char)*4096);
    memset(isTableExist,0,sizeof(char)*16);

    strncpy(query,"SELECT COUNT(*) FROM sqlite_master where type='table' and name='rewind_slow'",sizeof(char)*4095);
    sqlite3_exec(db,query,chk_table_exists_callback,isTableExist,NULL);

    if(strcmp(isTableExist,"0") == 0){
        memset(query,0,sizeof(char)*4096);
        strncpy(query," \
               CREATE TABLE rewind_slow ( \
                 path varchar(256) NOT NULL PRIMARY KEY, \
                 mtime varchar(256) NOT NULL \
                 )",sizeof(char)*4095);

        sqlite3_exec(db,query,NULL,NULL,NULL);
    }
    //释放内存
    free(isTableExist);
    free(query);

    return(0);
}

int chk_file_exist_from_db(const char *path,struct stat statbuf)
{
    static char *isPathExist;
    static char *isMtimeExist;
    isPathExist = (char *)malloc(sizeof(char)*16);
    isMtimeExist = (char *)malloc(sizeof(char)*16);
    memset(isPathExist,0,sizeof(char)*16);
    memset(isMtimeExist,0,sizeof(char)*16);

    char *query = NULL;

    sqlite3 *db;
    sqlite3_open("/etc/sysconfig/pdb/rewind.db",&db);

    query = (char *)malloc(sizeof(char)*4096);
    memset(query,0,sizeof(char)*4096);

    snprintf(query,sizeof(char)*4095,"SELECT COUNT(*) FROM rewind_slow where path='%s'",path);
    sqlite3_exec(db,query,chk_table_exists_callback,isPathExist,NULL);

    memset(query,0,sizeof(char)*4096);
    snprintf(query,sizeof(char)*4095,"SELECT COUNT(*) FROM rewind_slow where path='%s' and mtime='%d'",path,(int)statbuf.st_mtime);
    sqlite3_exec(db,query,chk_table_exists_callback,isMtimeExist,NULL);

    //如果文件路径不存在
    if(strcmp(isPathExist,"1") == 0){
        //如果文件路径存在，并且mtime不存在。
        if(strcmp(isMtimeExist,"0") == 0){
            free(isPathExist);
            free(isMtimeExist);
            return(2); 
        }
        else if(strcmp(isMtimeExist,"1") == 0){
            //如果文件路径存在，并且mtime存在。
            free(isPathExist);
            free(isMtimeExist);
            return(0);
        }
    }
    else{
        //如果文件路径不存在
        free(isPathExist);
        free(isMtimeExist);
        return(1);
    }
}

int update_filestatus_to_db(int rc,const char *path,struct stat statbuf)
{
     static char *isTableExist;
    isTableExist = (char *)malloc(sizeof(char)*16);

    char *query = NULL;

    sqlite3 *db;
    sqlite3_open("/etc/sysconfig/pdb/rewind.db",&db);

    query = (char *)malloc(sizeof(char)*4096);
    memset(query,0,sizeof(char)*4096);


    switch(rc){
        //如果文件存在并且，mtime存在
        case 0: 
            fprintf(stderr,"%s Have Read\n",path);
            goto goException;
            break;
        //如果文件路径不存在。
        case 1:
            snprintf(query,sizeof(char)*4095,"INSERT INTO  rewind_slow(path,mtime) values('%s','%d')",path,(int)statbuf.st_mtime);
            sqlite3_exec(db,query,NULL,NULL,NULL);
            goto goException;
            break;
            //如果文件路径存在，并且mtime不存在
        case 2:
            snprintf(query,sizeof(char)*4095,"UPDATE rewind_slow SET mtime='%d' WHERE path='%s'",(int)statbuf.st_mtime,path);
            sqlite3_exec(db,query,NULL,NULL,NULL);
            goto goException;
            break;
        default:
            fprintf(stderr,"Path && mtime");
            goto goException;
    }

goException:
    free(isTableExist);
    free(query);
    
}


//主函数    
    int
main (int   argc,char *argv[])
{
    //判断参数的正确性。
    if(argc <3){
        fprintf(stderr,"%s","Usage: \n\trewind_slow {path of slow log} {mongodb host:port}\n");
        fprintf(stderr,"Sample:\n\t\e[32m\e[1m%s\e[0m\n","rewind_slow /tmp  mongodb01:3307");
        exit(1);
    }

    char mongo_host_port[512] ={0};
    snprintf(mongo_host_port,sizeof(char)*511,"mongodb://%s",argv[2]);

    MC mc;
    int chk_ret= 0;

    DIR *dp = NULL;
    struct dirent *entry;
    struct stat statbuf;


    FILE *audit_fp = NULL;
    char audit_buf[8192] = {0};
    char audit_log_path[512] = {0};

    //创建mongodb连接实例。
    mongoc_init ();
    mc.client = mongoc_client_new (mongo_host_port);
    if(mc.client == NULL){
        fprintf(stderr,"\e[31m\e[1m%s\e[0m\n","mongoc_client_new Error");
        exit(3);
    }
    mc.database = mongoc_client_get_database (mc.client, "inspect");
    if(mc.database == NULL){
        fprintf(stderr,"\e[31m\e[1m%s\e[0m\n","mongoc_client_get_database Error");
        exit(3);
    }
    mc.collection = mongoc_client_get_collection (mc.client, "inspect", "slowlog");
    if(mc.collection == NULL){
        fprintf(stderr,"\e[31m\e[1m%s\e[0m\n","mongoc_client_get_collection Error");
        exit(3);
    }

    create_db_objects();


    //读取指定目录下的audit.log.xx文件。
    if(( dp = opendir(argv[1])) == NULL)
    {
        fprintf(stderr,"\e[31m\e[1m%s\e[0m\n","opendir Error");
        exit(4);
    }

    //切换到指定目录。
    chdir(argv[1]);

    while((entry = readdir(dp)) != NULL)
    {
        lstat(entry->d_name,&statbuf);
        //如果是普通文件就读取。
        if(S_IFREG &statbuf.st_mode)
        {
            //如果文件名是audit.log.xx就继续。
            if((strstr(entry->d_name,"slowquery.")) != NULL)
            {
                //拼出文件的绝对路径。 
                snprintf(audit_log_path,sizeof(char)*511,"%s/%s",argv[1],entry->d_name);
               
                chk_ret = chk_file_exist_from_db(audit_log_path,statbuf); 
                switch(chk_ret){
                    //如果文件路径存在并且mtime存在
                    case 0:
                        update_filestatus_to_db(chk_ret,audit_log_path,statbuf);
                        break;
                    //如果文件路径不存在并且mtime不存在
                    case 1:
                        //打开文件用于
                        audit_fp = fopen(audit_log_path,"r");
                        if(audit_fp == NULL){
                            fprintf(stderr,"\e[31m\e[1m%s\e[0m\n","fopen Error");
                            exit(1);
                        }


                        //插入前验证mongodb服务的可用性。
                        mc.command = BCON_NEW ("ping", BCON_INT32 (1));
                        mc.retval = mongoc_client_command_simple (mc.client, "admin", mc.command, NULL, &(mc.reply), &(mc.error));
                        if (!mc.retval) {
                            fprintf (stderr, "\e[31m\e[1m%s\e[0m\n", mc.error.message);
                            return EXIT_FAILURE;
                        }
                        else{
                            //如果服务可用，就把得到的内容打印到mongodb
                            while(fgets(audit_buf,sizeof(char)*8191,audit_fp) != NULL ){

                                mc.insert = bson_new_from_json((const uint8_t *)audit_buf,-1,&(mc.error));
                                if(mc.insert != NULL){
                                    if (!mongoc_collection_insert (mc.collection, MONGOC_INSERT_NONE, mc.insert, NULL, &(mc.error))){
                                        fprintf (stderr, "\e[31m\e[1m%s\e[0m\n", mc.error.message);
                                    }
                                }
                                //每次插入成功后把audit_buf清空。
                                memset(audit_buf,0,sizeof(char)*8191);
                            }

                        }
                        update_filestatus_to_db(chk_ret,audit_log_path,statbuf);
                        break;
                    //如果文件路径存在，但是mtime不相等。
                    case 2:
                        //打开文件用于
                        audit_fp = fopen(audit_log_path,"r");
                        if(audit_fp == NULL){
                            fprintf(stderr,"\e[31m\e[1m%s\e[0m\n","fopen Error");
                            exit(1);
                        }


                        //插入前验证mongodb服务的可用性。
                        mc.command = BCON_NEW ("ping", BCON_INT32 (1));
                        mc.retval = mongoc_client_command_simple (mc.client, "admin", mc.command, NULL, &(mc.reply), &(mc.error));
                        if (!mc.retval) {
                            fprintf (stderr, "\e[31m\e[1m%s\e[0m\n", mc.error.message);
                            return EXIT_FAILURE;
                        }
                        else{
                            //如果服务可用，就把得到的内容打印到mongodb
                            while(fgets(audit_buf,sizeof(char)*8191,audit_fp) != NULL ){

                                mc.insert = bson_new_from_json((const uint8_t *)audit_buf,-1,&(mc.error));
                                if(mc.insert != NULL){
                                    if (!mongoc_collection_insert (mc.collection, MONGOC_INSERT_NONE, mc.insert, NULL, &(mc.error))){
                                        fprintf (stderr, "\e[31m\e[1m%s\e[0m\n", mc.error.message);
                                    }
                                }
                               //每次插入成功后把audit_buf清空。
                                memset(audit_buf,0,sizeof(char)*8191);
                            }

                        }
                        update_filestatus_to_db(chk_ret,audit_log_path,statbuf);
                        break;
                    default:
                        fprintf(stderr,"\e[31m\e[1m%s\e[0m\n","chk Error");
                        exit(2);
                }

            }
        }
    }

    //bson_destroy (mc.insert);
    //bson_destroy (mc.command);

    mongoc_collection_destroy (mc.collection);
    mongoc_database_destroy (mc.database);
    mongoc_client_destroy (mc.client);
    mongoc_cleanup ();

    return 0;
}
