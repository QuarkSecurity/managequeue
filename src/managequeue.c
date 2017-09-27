#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <fcntl.h>
#include <libconfig.h>

const short int PROJ_ID = 15;

struct msg_queue_config {
	char* path;
	int permissions;
  uid_t user_id;
  gid_t group_id;
};

/* Creates a new msg queue
 *
 * @param msgq_conf       pointer to the msg queue config struct.
 * @return 0 for success -1 for failure
 */
int create_msg_queue(struct msg_queue_config *msgq_conf){
  struct msqid_ds mq_buffer;
  int ret = -1;
  int msgqueue_id;
  int msgflg = (IPC_CREAT | msgq_conf->permissions);
  key_t key;

  if(create_msg_queue_file(msgq_conf->path, msgq_conf->permissions, msgq_conf->user_id, msgq_conf->group_id) != 0){
    fprintf(stderr,"Failed to create file\n");
    goto done;
  }

  key = ftok(msgq_conf->path, PROJ_ID);
  if (key == -1){
    fprintf(stderr, "An error has occured with ftok()\n");
    goto done;
  }
  msgqueue_id = msgget(key, msgflg);
  if (msgqueue_id == -1){
    fprintf(stderr,"An error has occured creating the message queue\n");
    goto done;
  }

  if (msgq_conf->user_id != -1 || msgq_conf->group_id != -1) {

    if (msgctl(msgqueue_id, IPC_STAT, &mq_buffer) < 0) {
        fprintf(stderr,"Failed to get message queue.\n");
        goto done;
    }

    if(msgq_conf->user_id != -1){
      mq_buffer.msg_perm.uid = msgq_conf->user_id;
    }
    if(msgq_conf->group_id != -1){
      mq_buffer.msg_perm.gid = msgq_conf->group_id;
    }

    if (msgctl(msgqueue_id, IPC_SET, &mq_buffer) < 0) {
        fprintf(stderr,"Failed to save messagequeue\n");
        goto done;
    }
  }

  ret = 0;
done:
  return ret;
}

/* Creates the output path of the ms_queue_file
 *
 * @param path            pointer to the path
 * @param permissions     a decimal representation of the permissions for the file
 * @param user_id         id of the user who owns the file
 * @param group_id         id of the group who rights to the file
 * @return 0 for success -1 for failure
 */
int create_msg_queue_file(char *path, int permissions, uid_t user_id, gid_t group_id){
  int ret = -1;
  // Makes sure the path doesn't go back any directories
  if (strstr(path, "..")){
    printf("Invalid path found\n");
    goto done;
  }

  if (strncmp("/var/run/", path, 9)){
    printf("The messagequeue must be located in the \"/var/run/\" directory.\n");
    goto done;
  }
  // Checks that the size of the path doesn't exceed PATH_MAX
  if (strlen(path) > PATH_MAX - 1){
    printf("The length of the path is too long.\n");
    goto done;
  }

  if (mkdir_p(path) != 0){
    fprintf(stderr,"Failed to create path\n");
    goto done;
  }

  int file_descriptor = open(path, O_CREAT | O_RDWR , permissions);

  if (file_descriptor < 0) {
      fprintf(stderr,"Failed to create file.\n");
      goto done;
  }

  if (fchmod(file_descriptor, permissions) != 0) {
      fprintf(stderr,"Failed to set permissions\n");
      goto done;
  }

  if (user_id != -1 || group_id != -1) {
      if (fchown(file_descriptor, user_id, group_id) < 0) {
          fprintf(stderr,"Failed to set file owner.\n");
          goto done;
      }
  }

  ret = 0;
done:
  return ret;

}

/* Makes all of the directories in a path if they don't exist
 *
 * @param path            pointer to the path
 * @return 0 for success -1 for failure
 */
int mkdir_p(char *path){
  int ret = -1;
  char *path_cpy, *tmp_path, *tok, *nxt_tok, *sptr;

  tmp_path  = (char *)malloc(PATH_MAX);
  memset(tmp_path, 0, PATH_MAX);
  path_cpy = strdup(path);
  strncat(tmp_path, "/", PATH_MAX);
  tok = strtok_r(path_cpy, "/", &sptr);
  if (tok == NULL) {
      fprintf(stderr, "Failed to get token");
      goto done;
  }

  while((nxt_tok = strtok_r(NULL, "/", &sptr)) != NULL){

    strncat(tmp_path, tok, PATH_MAX - strlen(tmp_path));
    strncat(tmp_path, "/", PATH_MAX - strlen(tmp_path));

    if (mkdir(tmp_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) { /* Error. */
      if (errno != EEXIST) {
          goto done;
      }
    }
    tok = nxt_tok;
  }

  ret = 0;
done:
  if(tmp_path){
    free(tmp_path);
  }

  return ret;
}

/* Deletes the msg queue and the file its file
 *
 * @param msgq_conf       pointer to the msg queue config struct.
 * @return 0 for success -1 for failure
 */
int delete_msg_queue(struct msg_queue_config *msgq_conf){
  int ret = -1;
  int msgqueue_id;
  key_t key;

  key = ftok(msgq_conf->path, PROJ_ID);
  if (key == -1){
    fprintf(stderr,"An error has occured with ftok()\n");
    goto done;
  }
  msgqueue_id = msgget(key, 0);
  if (msgqueue_id == -1){
    fprintf(stderr,"An error has occured creating the message queue\n");
    goto done;
  }

  if (msgctl(msgqueue_id, IPC_RMID, NULL) == -1){
    fprintf(stderr,"An error has occured while removing the message queue.\n");
    goto done;
  }

  if (remove(msgq_conf->path) == -1){
    fprintf(stderr,"An error has occured while removing the message queue.\n");
    goto done;
  }

  ret = 0;
done:
  // validate
  return ret;
}


int load_config(char *config, struct msg_queue_config* msgq_conf){
  int ret = -1;
  config_t cfg;
  const char *path = NULL, *username = NULL, *group = NULL;
  int permissions;
  config_init(&cfg);

  if (config_read_file(&cfg, config) != CONFIG_TRUE) {
    fprintf(stderr,"Failed to read config file.\n");
    printf("%s\n", config_error_text(&cfg));
    goto done;
  }

  if(config_lookup_string(&cfg, "path", &path) != CONFIG_TRUE){
    fprintf(stderr,"Failed to find path: %s\n", config_error_text(&cfg));
    goto done;
  }

  if(config_lookup_int(&cfg, "permissions", &permissions) != CONFIG_TRUE){
    fprintf(stderr,"Failed to find permissions: %s\n", config_error_text(&cfg));
    goto done;
  }

  if(config_lookup_string(&cfg, "username", &username) != CONFIG_TRUE){
  }

  if(config_lookup_string(&cfg, "group", &group) != CONFIG_TRUE){
  }

  if (username != NULL){
    struct passwd *p;

    p = getpwnam(username);
    if(p == NULL){
      fprintf(stderr,"Invalid username\n");
      goto done;
    }
    msgq_conf->user_id = p->pw_uid;
  } else {
    msgq_conf->user_id = -1;
  }

  if (group != NULL){
    struct group *g;

    g = getgrnam(group);
    if(g == NULL){
      fprintf(stderr,"Invalid group name\n");
      goto done;
    }
    msgq_conf->group_id = g->gr_gid;
  } else {
    msgq_conf->group_id = -1;
  }

  msgq_conf->path = (char *)malloc(strlen(path));
  msgq_conf->path = strdup(path);
  msgq_conf->permissions = oct_to_dec(permissions);

  ret = 0;
done:
  config_destroy(&cfg);
  return ret;
}


/* loads the parameters into the msg_queue_config struct.
 * @param num_args        number of arguements being passsed
 * @param args            array of arguments
 * @param [out] msgq_conf       pointer to the msg queue config struct.
 * @return 0 for success -1 for failure
 */
int load_parameters(int num_args, char **args, struct msg_queue_config* msgq_conf){
  int ret = -1;
  char *username = NULL, *group = NULL;
  // copies over the path
  msgq_conf->path = (char *)malloc(strlen(args[2]));
  strcpy(msgq_conf->path, args[2]);

  // Checks the permissions are within correct range.
  if (num_args > 3){
    int permissions = atoi(args[3]);
    if (permissions < 0 || permissions > 777){
      printf("Invalid permissions\n");
      goto done;
    }
    msgq_conf->permissions = oct_to_dec(permissions);
  }

  if (num_args > 4){
    struct passwd *p;
    username = (char *)malloc(strlen(args[4]));
    strcpy(username, args[4]);

    p = getpwnam(username);
    if(p == NULL){
      fprintf(stderr,"Invalid username\n");
      goto done;
    }
    msgq_conf->user_id = p->pw_uid;
    free(username);
  } else {
    msgq_conf->user_id = -1;
  }

  if (num_args > 5){
    struct group *g;
    group = (char *)malloc(strlen(args[5]));
    strcpy(group, args[5]);

    g = getgrnam(group);
    if(g == NULL){
      fprintf(stderr,"Invalid group name\n");
      goto done;
    }
    msgq_conf->group_id = g->gr_gid;
    free(group);
  } else {
    msgq_conf->group_id = -1;
  }
  ret = 0;
done:
  return ret;
}


/* cleans up msg queue config struct
 *
 * @param msgq_conf       pointer to the msg queue config struct.
 * @return 0 for success -1 for failure
 */
void config_cleanup(struct msg_queue_config* msgq_conf){
  if(msgq_conf->path){
    free(msgq_conf->path);
  }
}


/* Converts an octal value into a decimal representation
 *
 * @param oct         A value in octal
 * @return decimal representation of the octal value
 */
int oct_to_dec(int oct){
  int dec = 0, pow8 = 1;
  while(oct != 0){
    dec = dec + ((oct % 10) * pow8);
    oct = oct / 10;
    pow8 = pow8 * 8;
  }
  return dec;
}


int main(int argc, char **argv)
{
  int ret = -1, opt;
  char *cfg_path = NULL;
  bool is_config = false;
  struct msg_queue_config msgq_conf;
  char *cmd = argv[1];

  setvbuf(stdout, NULL, _IONBF, 0);

  while ((opt = getopt(argc, argv, "c:")) != -1) {
      switch (opt) {
          case 'c':
              cfg_path = strdup(optarg);
              is_config = true;
              break;
          default:
              break;
      }
  }

  if (is_config){
    load_config(cfg_path, &msgq_conf);
  } else if (argc >= 3){
    load_parameters(argc, argv, &msgq_conf);
  } else {
    printf("Invalid parameters\n");
    printf("Usage:\n");
    printf("\tOnly Command Parameters: %s <create/delete> <path> <permissions> [username] [group]\n", argv[0]);
    printf("\tWith Config:             %s <create/delete> -c <config file>\n", argv[0]);

    sd_notify(0, "STOPPING=1\nSTATUS=Error invalid parameters");
    goto done;
  }

  if (strcmp(cmd,"create") == 0 || strcmp(cmd,"CREATE") == 0){
    if(create_msg_queue(&msgq_conf) == -1){
        sd_notify(0, "STOPPING=1\nSTATUS=Error failed to create queue");
        goto done;
    }
    sd_notify(0, "READY=1\nSTATUS=Created Queue");
  } else if (strcmp(cmd,"delete") == 0 || strcmp(cmd,"DELETE") == 0){
    if(delete_msg_queue(&msgq_conf)){
        sd_notify(0, "STOPPING=1\nSTATUS=Error failed to delete queue");
        goto done;
    }
    sd_notify(0, "READY=1\nSTATUS=Queue Deleted");
  } else {
    fprintf(stderr,"Invalid command\n");
    sd_notify(0, "STOPPING=1\nSTATUS=Invalid command");
  }

  ret = 0;
done:
  config_cleanup(&msgq_conf);
  return ret;
}
