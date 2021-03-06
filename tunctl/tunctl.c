/* Copyright 2002 Jeff Dike
 * Licensed under the GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>

/* TUNSETGROUP appeared in 2.6.23 */
#ifndef TUNSETGROUP
#define TUNSETGROUP   _IOW('T', 206, int)
#endif

static void Usage(char *name)
{
  fprintf(stderr, "Create: %s [-b] [-n [-m]] [-u owner] [-g group] [-t device-name] "
	  "[-f tun-clone-device]\n", name);
  fprintf(stderr, "Delete: %s -d device-name [-f tun-clone-device]\n\n",
	  name);
  fprintf(stderr, "The default tun clone device is /dev/net/tun - some systems"
	  " use\n/dev/misc/net/tun instead\n\n");
  fprintf(stderr, "\t-b will result in brief output (just the device name)\n");
  fprintf(stderr, "\t-n will create a non-persistent tunnel, and sleep until killed\n");
  fprintf(stderr, "\t-m will daemonize before slleeping, only makes sense with -n\n");

  exit(1);
}

int main(int argc, char **argv)
{
  struct ifreq ifr;
  struct passwd *pw;
  struct group *gr;
  uid_t owner = -1;
  gid_t group = -1;
  int tap_fd, opt, delete = 0, brief = 0, persistent = 1, daemonize = 0;
  char *tun = "", *file = "/dev/net/tun", *name = argv[0], *end;

  while((opt = getopt(argc, argv, "bd:f:t:u:g:nm")) > 0){
    switch(opt) {
      case 'b':
        brief = 1;
        break;
      case 'd':
        delete = 1;
	tun = optarg;
        break;
      case 'f':
	file = optarg;
	break;
      case 'u':
	pw = getpwnam(optarg);
	if(pw != NULL){
	  owner = pw->pw_uid;
	  break;
	}
        owner = strtol(optarg, &end, 0);
	if(*end != '\0'){
	  fprintf(stderr, "'%s' is neither a username nor a numeric uid.\n",
		  optarg);
	  Usage(name);
	}
        break;
      case 'g':
	gr = getgrnam(optarg);
	if(gr != NULL){
	  group = gr->gr_gid;
	  break;
	}
        group = strtol(optarg, &end, 0);
	if(*end != '\0'){
	  fprintf(stderr, "'%s' is neither a groupname nor a numeric group.\n",
		  optarg);
	  Usage(name);
	}
        break;

      case 't':
        tun = optarg;
        break;
      case 'n':
        persistent = 0;
        break;
      case 'm':
        daemonize = 1;
        break;

      case 'h':
      default:
        Usage(name);
    }
  }

  argv += optind;
  argc -= optind;

  if(argc > 0)
    Usage(name);

  if((tap_fd = open(file, O_RDWR)) < 0){
    fprintf(stderr, "Failed to open '%s' : ", file);
    perror("");
    exit(1);
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strncpy(ifr.ifr_name, tun, sizeof(ifr.ifr_name) - 1);
  if(ioctl(tap_fd, TUNSETIFF, (void *) &ifr) < 0){
    perror("TUNSETIFF");
    exit(1);
  }

  if(delete){
    if(ioctl(tap_fd, TUNSETPERSIST, 0) < 0){
      perror("disabling TUNSETPERSIST");
      exit(1);
    }
    printf("Set '%s' nonpersistent\n", ifr.ifr_name);
  }
  else {
    /* emulate behaviour prior to TUNSETGROUP */
    if(owner == -1 && group == -1) {
      owner = geteuid();
    }

    if(owner != -1) {
      if(ioctl(tap_fd, TUNSETOWNER, owner) < 0){
      	perror("TUNSETOWNER");
      	exit(1);
      }
    }
    if(group != -1) {
      if(ioctl(tap_fd, TUNSETGROUP, group) < 0){
      	perror("TUNSETGROUP");
      	exit(1);
      }
    }

    if (persistent) {
      if(ioctl(tap_fd, TUNSETPERSIST, 1) < 0){
        perror("enabling TUNSETPERSIST");
        exit(1);
      }
    }

    if(brief)
      printf("%s\n", ifr.ifr_name);
    else {
      if (persistent) {
        printf("Set '%s' persistent and owned by", ifr.ifr_name);
      } else {
        printf("(%u) Sleeping on non-persistent '%s' owned by",
                        getpid(), ifr.ifr_name);
      }
      if(owner != -1)
          printf(" uid %d", owner);
      if(group != -1)
          printf(" gid %d", group);
      printf("\n");
    }

    if (!persistent) {
          if (daemonize)
            daemon(1, 1);
          pause();
    }
  }
  return(0);
}
