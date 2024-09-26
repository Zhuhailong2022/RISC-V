# Lab11.Network

这个任务是为网卡接口实现一个设备驱动程序。

## 实验相关

**网络协议栈**

从应用程序到网络上传输的数据帧是数据经过了TCP/IP模型的逐层加密形成的，在传输层，给应用数据前面增加了TCP头；在网络层，给TCP增加了IP头；在网络接口层，给IP数据包前后分别增加了帧头和帧尾。

操作系统接收并处理网络包的流程：

1.网卡负责接收网络上的数据包，接收之后使用DMA技术直接将网络数据包写入指定的内存地址（Ring Buffer环形缓冲区），然后触发操作系统中断，告诉网络数据包的到达。

2.硬中断触发后会执行硬中断处理函数，这里会“暂时屏蔽网卡的硬件中断”，这样网卡再次接收到数据包后直接写道内存指定位置，不再通知CPU这样可以提高效率，避免CPU一直被不停中断。接着发起软中断。

3.软中断是由内核中的专门线程进行处理，当软中断发生时这个线程会从 Ring Buffer 中获取数据包并交给网络协议栈进行逐层处理。

4.网络协议栈中

* 网络接口层：这一层中检查报文的合法性，不合法则丢弃，合法则找出网络包的上层协议比如IPV4还是IPV6，接着去掉帧头和帧尾。
* 网络层：取出IP包，判断网络包下一步的走向。比如是交给传输层处理还是转发出去（arp协议包）。当确认这个包是要发送本机后，就会从IP头里看看上一层协议的类型是TCP还是UDP，接着去掉IP头，然后交给传输层。如果是ARP协议的数据包，则封装ARP数据包交给网络接口层封装并发回到网络中。（获取MAC地址的方法）
* 传输层：取出TCP/UDP头，根据四元组“源IP、端口和目的IP、端口”作为标识找到对应的 SOCKET，并把数据放到 SOCKET 的接收缓冲区。
* 最后应用程序调用 read_socket接口，将内核中 SOCKET 接收缓冲区的数据拷贝到应用层的缓冲区中。

  ![](https://cdn.xiaolincoding.com/gh/xiaolincoder/ImageHost3@main/%E6%93%8D%E4%BD%9C%E7%B3%BB%E7%BB%9F/%E6%B5%AE%E7%82%B9/%E6%94%B6%E5%8F%91%E6%B5%81%E7%A8%8B.png)

## 实现思路

任务要是现的两个函数 e1000_transmit 的作用是，将应用层经过协议栈产生的数据帧`struct mbuf *m`放入 Ring Buffer中。而另一个函数 e1000_recv 的作用是在操作系统中断被触发时，操作系统从 Ring Buffer 环中取出数据帧的。

### e1000_transmite的实现

* 发送数据帧的时候，多个进程可能都有数据帧需要发送，这里存在并发关系，对于描述符环和缓存区这两个临界资源需要加锁。
* 描述符必要的cmd标志位有两个，E1000\_TXD\_CMD\_EOP 和 E1000\_TXD\_CMD\_RS，EOP表示数据包的结束，RS表示status字段有效，故二者都需要被设置。

```c
int e1000_transmit(struct mbuf *m)
{
  uint32 rear;//指向下一个需要发送的数据包

  acquire(&e1000_lock);
  // 1.获取下一个需要发送的数据包在环中的索引
  rear = regs[E1000_TDT];
  // 2.检查数据块是否带有E1000_TXD_STAT_DD标志，若无则数据还未完成转发
  if ((tx_ring[rear].status & E1000_TXD_STAT_DD) == 0)
  {
    release(&e1000_lock);
    return -1;
  }
  // 3.释放已转发的数据块
  if (tx_mbufs[rear])
  {
    mbuffree(tx_mbufs[rear]);
  }
  // 4.设置描述符与缓存区字段
  tx_ring[rear].addr = (uint64)m->head;
  tx_ring[rear].length = m->len;
  tx_ring[rear].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  tx_mbufs[rear] = m;
  // 5.修改环尾索引
  regs[E1000_TDT] = (rear + 1) % TX_RING_SIZE;

  release(&e1000_lock);

  return 0;
}

```


### e1000_recv实现

* 接收数据帧的e1000_recv是由中断驱动的，处理完才会返回，因此不存在并发关系，不需要加锁
* 由hints提示可知，此处尾指针指向的是已被软件处理的数据帧, 其下一个才为当前需要处理的数据帧，因此索引需要加1
  为了解决接收队列的缓存区溢出，超出上限的问题，采用了循环读取，每次尽量将队列读空。


  ```c
  static void
  e1000_recv(void)
  {
    // 1.获取下一个需要接收的数据包在环中的索引
    uint32 rear = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    // 2.检查E1000_TXD_STAT_DD标志
    while ((rx_ring[rear].status & E1000_TXD_STAT_DD))
    {
      if (rx_ring[rear].length > MBUF_SIZE)
      {
        panic("E1000 length overflow");
      }
      // 3.更新缓冲块信息，递交数据包给网络栈解封装
      rx_mbufs[rear]->len = rx_ring[rear].length;
      net_rx(rx_mbufs[rear]);
      // 4.分配新的缓存区，更新描述符
      rx_mbufs[rear] = mbufalloc(0);
      rx_ring[rear].addr = (uint64)rx_mbufs[rear]->head;
      rx_ring[rear].status = 0;

      rear = (rear + 1) % RX_RING_SIZE;
    }
    // 5.修改环尾索引
    // 此处由于while循环末端让rear = rear - 1了，所以尾指针索引需要减1
    // 若没有该循环，则此处不需要修改rear，因为尾指针指向的是已被软件处理的数据帧
    regs[E1000_RDT] = (rear - 1) % RX_RING_SIZE;
  }

  ```
