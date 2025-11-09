#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;


// 数据包节点结构体
struct packet_node {
  char *buf;            // 数据包缓冲区
  int len;              // 数据包长度
  struct packet_node *next; // 下一个节点
};

// 数据包队列结构体
struct packetq {
  struct packet_node *head;
  struct packet_node *tail;
};

// 端口数据结构，用于跟踪绑定的端口和等待的数据包
struct sock {
  struct spinlock lock;  // 保护这个结构体的锁
  uint16 rport;          // 接收端口
  struct packetq q;      // 数据包队列
  int used;              // 标记端口是否已使用
};

// 全局端口表，最多支持 16 个端口
struct sock socks[16];

// 初始化数据包队列
void
packetq_init(struct packetq *q)
{
  q->head = 0;
  q->tail = 0;
}

// 向队列尾部添加一个数据包
void
packetq_push(struct packetq *q, char *buf, int len)
{
  struct packet_node *node = (struct packet_node*)kalloc();
  if(!node)
    return; // 内存分配失败，丢弃数据包
  
  node->buf = buf;
  node->len = len;
  node->next = 0;
  
  if(q->head == 0) {
    q->head = node;
  } else {
    q->tail->next = node;
  }
  q->tail = node;
}

// 从队列头部取出一个数据包
struct packet_node*
packetq_pop(struct packetq *q)
{
  struct packet_node *node = q->head;
  if(node) {
    q->head = node->next;
    if(q->head == 0) {
      q->tail = 0;
    }
  }
  return node;
}

// 检查队列是否为空
int
packetq_empty(struct packetq *q)
{
  return q->head == 0;
}

// 计算队列中的数据包数量
int
packetq_len(struct packetq *q)
{
  int n = 0;
  struct packet_node *node = q->head;
  while(node) {
    n++;
    node = node->next;
  }
  return n;
}

void
netinit(void)
{
  initlock(&netlock, "netlock");
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  int port;
  
  // 获取端口号参数
  argint(0, &port);
    
  // 查找可用的端口表项
  for(int i = 0; i < 16; i++) {
    acquire(&socks[i].lock);
    if(socks[i].used == 0) {
      // 标记为已使用并设置端口号
      socks[i].used = 1;
      socks[i].rport = port;
      release(&socks[i].lock);
      return 0;
    }
    release(&socks[i].lock);
  }
  
  // 没有可用的端口表项
  return -1;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  int dport;
  uint64 src_addr, sport_addr, buf_addr;
  int maxlen;
  
  // 获取参数
  argint(0, &dport);
  argaddr(1, &src_addr);
  argaddr(2, &sport_addr);
  argaddr(3, &buf_addr);
  argint(4, &maxlen);
  
  // 查找绑定的端口
  struct sock *s = 0;
  for(int i = 0; i < 16; i++) {
    acquire(&socks[i].lock);
    if(socks[i].used && socks[i].rport == dport) {
      s = &socks[i];
      break;
    }
    release(&socks[i].lock);
  }
  
  if(!s)
    return -1; // 端口未绑定
  
  // 等待数据包到达
  while(packetq_empty(&s->q)) {
    sleep(&s->q, &s->lock);
  }
  
  // 取出数据包
  struct packet_node *node = packetq_pop(&s->q);
  if(!node) {
    release(&s->lock);
    return -1; // 不应该发生
  }
  
  // 解析数据包
  struct eth *eth = (struct eth *)node->buf;
  struct ip *ip = (struct ip *)(eth + 1);
  struct udp *udp = (struct udp *)(ip + 1);
  
  // 获取源 IP 和源端口
  uint32 src_ip = ntohl(ip->ip_src);
  uint16 src_port = ntohs(udp->sport);
  
  // 计算有效载荷长度和位置
  int payload_len = ntohs(udp->ulen) - sizeof(struct udp);
  char *payload = (char *)(udp + 1);
  
  // 限制复制长度
  if(payload_len > maxlen)
    payload_len = maxlen;
  
  // 复制数据到用户空间
  if(copyout(myproc()->pagetable, buf_addr, payload, payload_len) < 0) {
    kfree(node->buf);
    kfree(node);
    release(&s->lock);
    return -1;
  }
  
  // 复制源 IP 和源端口到用户空间
  if(copyout(myproc()->pagetable, src_addr, (char*)&src_ip, sizeof(src_ip)) < 0 ||
     copyout(myproc()->pagetable, sport_addr, (char*)&src_port, sizeof(src_port)) < 0) {
    kfree(node->buf);
    kfree(node);
    release(&s->lock);
    return -1;
  }
  
  // 释放资源
  kfree(node->buf);
  kfree(node);
  release(&s->lock);
  
  return payload_len;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  // 检查数据包长度是否足够包含以太网头、IP头和UDP头
  if(len < sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp)) {
    kfree(buf);
    return;
  }
  
  // 解析数据包
  struct eth *eth = (struct eth *)buf;
  struct ip *ip = (struct ip *)(eth + 1);
  
  // 检查是否是UDP协议
  if(ip->ip_p != IPPROTO_UDP) {
    kfree(buf);
    return;
  }
  
  struct udp *udp = (struct udp *)(ip + 1);
  uint16 dport = ntohs(udp->dport);
  
  // 查找绑定的端口
  struct sock *s = 0;
  for(int i = 0; i < 16; i++) {
    acquire(&socks[i].lock);
    if(socks[i].used && socks[i].rport == dport) {
      s = &socks[i];
      break;
    }
    release(&socks[i].lock);
  }
  
  if(!s) {
    // 端口未绑定，丢弃数据包
    kfree(buf);
    return;
  }
  
  // 检查队列是否已满（最多16个数据包）
  if(packetq_len(&s->q) >= 16) {
    // 队列已满，丢弃数据包
    release(&s->lock);
    kfree(buf);
    return;
  }
  
  // 将数据包加入队列
  packetq_push(&s->q, buf, len);
  
  // 唤醒等待的进程
  wakeup(&s->q);
  
  release(&s->lock);
  
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
