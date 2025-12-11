#define PASSWD_PATH "/password.txt"

struct passwd {
  // TODO: implement fields into passwd
};

// taken from man://pwd.h (POSIX)
struct passwd *getpwent(void);
void setpwent(void);
void endpwent(void);
struct passwd *getpwnam(const char *);
// int getpwnam_r(const char *, struct passwd *, char *, size_t, struct passwd **);
struct passwd *getpwuid(uint);
// int getpwuid_r(uid_t, struct passwd *, char *, size_t, struct passwd **);
int putpwent(const struct passwd *);