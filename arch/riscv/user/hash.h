#ifndef _USER_HASH_H
#define _USER_HASH_H

// 随机数生成器
unsigned int hash_rand(void);

// Jenkins one-at-a-time哈希函数
unsigned int jenkins_one_at_a_time_hash(const char* key, unsigned int length);

// 生成加盐哈希密码
// 格式: $hash$salt
void generate_hashed_password(const char* password, char* output);

// 使用指定盐值生成哈希密码
void generate_hashed_password_with_salt(const char* password, unsigned int salt, char* output);

#endif