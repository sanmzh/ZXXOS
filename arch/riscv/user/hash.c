#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/hash.h"

// 简单的随机数生成器，基于uptime和getpid
unsigned int hash_rand(void) {
  static unsigned int seed = 0;
  
  if (seed == 0) {
    // 使用uptime和getpid作为种子
    seed = uptime() ^ getpid();
  }
  
  // 简单的线性同余生成器
  seed = seed * 1103515245 + 12345;
  return (seed / 65536) % 32768;
}

// Jenkins one-at-a-time哈希函数
unsigned int jenkins_one_at_a_time_hash(const char* key, unsigned int length) {
  unsigned int i = 0;
  unsigned int hash = 0;
  
  while (i != length) {
    hash += key[i++];
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  
  return hash;
}

// 生成加盐哈希密码
// 格式: $hash$salt
void generate_hashed_password(const char* password, char* output) {
  // 生成随机盐
  unsigned int salt = hash_rand();
  
  // 将密码和盐连接
  char salted_password[128];
  strcpy(salted_password, password);
  
  char salt_str[16];
  itoa(salt, salt_str, 10);
  strcat(salted_password, salt_str);
  
  // 计算哈希值
  unsigned int hash = jenkins_one_at_a_time_hash(salted_password, strlen(salted_password));
  
  // 格式化输出: $hash$salt
  strcpy(output, "$");
  char hash_str[16];
  itoa(hash, hash_str, 10);
  strcat(output, hash_str);
  strcat(output, "$");
  strcat(output, salt_str);
}

// 使用指定盐值生成哈希密码
void generate_hashed_password_with_salt(const char* password, unsigned int salt, char* output) {
  // 将密码和盐连接
  char salted_password[128];
  strcpy(salted_password, password);
  
  char salt_str[16];
  itoa(salt, salt_str, 10);
  strcat(salted_password, salt_str);
  
  // 计算哈希值
  unsigned int hash = jenkins_one_at_a_time_hash(salted_password, strlen(salted_password));
  
  // 格式化输出: $hash$salt
  strcpy(output, "$");
  char hash_str[16];
  itoa(hash, hash_str, 10);
  strcat(output, hash_str);
  strcat(output, "$");
  strcat(output, salt_str);
}