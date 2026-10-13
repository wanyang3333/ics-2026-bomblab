Lab2 BombLab 实验报告

赵婉扬
25800190042

（一）实验环境--------------------------------------------------------

Ubuntu (WSL2)，Linux x86_64。用到的工具有 GCC、GNU Make、GDB、objdump。分析方式主要是先用 objdump 把关键函数反汇编出来看，再在 GDB 里实际运行验证公式。

（二）第一关--------------------------------------------------------

1. 关键汇编

phase_01 入口部分：

  40202a:	mov    %rsi,%r12             ; r12 = challenge
  402033:	mov    %rdi,%rbx             ; rbx = 输入字符串
  40203a:	call   valid_u32_fields
  402052:	lea    -0x14(%rbp),%rdx       ; 输入整数存放位置
  402056:	mov    $0x403182,%esi         ; sscanf 格式串
  40205e:	call   __isoc99_sscanf@plt
  402063:	cmp    $0x1,%eax              ; 必须解析出 1 个字段
  402068:	mov    %r12,%rdi             ; rdi = challenge
  40206b:	call   p1_compute
  402070:	cmp    %eax,-0x14(%rbp)       ; 输入 == 返回值？
  402073:	sete   %al

p1_compute：

  401fe5:	mov    %rdi,%rax
  401feb:	mov    %edi,%esi             ; esi = 低 32 位
  401fed:	shr    $0x20,%rax             ; rax = challenge >> 32
  401ff4:	shr    $0x10,%rdx             ; rdx = challenge >> 16
  401ff8:	mov    %eax,%edi             ; edi = 高 32 位
  401ffd:	call   p1_step2
  402002:	mov    %rdx,%rsi
  402005:	mov    %r8d,%edi
  402008:	mov    %eax,%ecx             ; 暂存 step2 结果
  40200a:	call   p1_step0
  40200f:	mov    %ecx,%esi
  402011:	mov    %eax,%edi
  402013:	call   p1_step1

三个小函数：

  p1_step0: lea 0xd4f6(%rsi),%eax ; xor %edi,%eax
  p1_step1: sub %esi,%edi         ; lea 0x37c9(%rdi,%rdi,2),%eax
  p1_step2: lea (%rdi,%rsi,8),%eax ; xor $0x448c,%eax

2. 推理过程

先看 phase_01 最后那段，cmp %eax,-0x14(%rbp) 后面跟着 sete %al。eax 是 p1_compute 的返回值，-0x14(%rbp) 是 sscanf 读进来的那个整数。两个相等 al 才会被置 1，这一关才通过。所以这一关要输入的整数就是 p1_compute(challenge) 的返回值。

再看入口处 sscanf 返回后 cmp $0x1,%eax，返回值必须为 1，说明本关只需要一个整数。

接着分析 p1_compute。函数一开始 rdi 就是 challenge。它做了几件事：把 rdi 复制到 rax 和 rdx；取 edi 作为低 32 位；把 rax 右移 32 位得到高 32 位；把 rdx 右移 16 位得到 challenge >> 16。所以传入三个辅助函数的参数分别是低 32 位、高 32 位和中间部分。

调用顺序是 step2、step0、step1。step2 用高 32 位和低 32 位算出 rdi + rsi*8，再异或 0x448c，结果保存在 ecx 里等会儿用。step0 用低 32 位和 challenge >> 16 算出 (rsi + 0xd4f6) ^ rdi。step1 拿 step0 的结果减去 step2 的结果，乘 3 再加 0x37c9。

所有运算都是 32 位无符号，加法和乘法溢出时自动对 2^32 取模。用 C 写的时候用 uint32_t，不要用 int 或 uint64_t。

3. 计算关系

low = (uint32_t)challenge;
high = (uint32_t)(challenge >> 32);
mid = (uint32_t)(challenge >> 16);
step2 = (high + low * 8) ^ 0x448c;
step0 = (mid + 0xd4f6) ^ low;
answer = 3 * (step0 - step2) + 0x37c9;

4. 验证

./defuser --seed 43 --phase 1 | ./bomb --seed 43 --phase 1
输出 Phase 1 defused.

截图：images/phase1.png

（三）第二关----------------------------------------------------------

1. 关键汇编

phase_02 入口：

  40208d:	mov    $0x6,%esi               ; 校验 6 个字段
  40209c:	call   valid_u32_fields
  4020de:	call   __isoc99_sscanf@plt
  4020e5:	cmp    $0x6,%eax
  4020ea:	mov    %r13,%rax               ; rax = challenge
  4020ed:	mov    -0x40(%rbp),%esi        ; 第 1 个输入
  4020f0:	shr    $0x29,%rax               ; challenge >> 41
  4020f4:	movzbl %al,%eax                 ; 取低 8 位
  4020f7:	xor    $0x7,%eax                 ; ^ 7
  4020fa:	cmp    %eax,%esi
  4020fc:	jne    4020a5

递推循环：

  4020fe:	mov    $0x13,%ecx               ; ecx = 19
  402103:	mov    $0x1,%edx                ; edx = i = 1
  402108:	mov    %r13,%rax
  40210b:	shr    %cl,%rax                 ; challenge >> (19 + 3*(i-1))
  40210e:	and    $0xf,%eax                 ; 取低 4 位
  402111:	xor    %edx,%eax                 ; ^ i
  402113:	lea    (%rax,%rsi,2),%eax        ; + 2 * 前一项
  402116:	mov    -0x40(%rbp,%rdx,4),%esi
  40211a:	cmp    %eax,%esi
  40211c:	jne    4020a5
  40211e:	add    $0x1,%rdx
  402122:	add    $0x3,%ecx
  402125:	cmp    $0x6,%rdx
  402129:	jne    402108

2. 推理过程

入口处 esi = 6 传给了 valid_u32_fields，后面 sscanf 返回值也必须等于 6。所以这一关要输入六个整数。

第一个整数单独处理。challenge 右移 41 位之后 movzbl 只保留最低一字节，相当于 (challenge >> 41) & 0xFF，再异或 7 就是第一个整数。写成 C 是 v0 = ((challenge >> 41) & 0xFF) ^ 7。

从 4020fe 开始是处理剩下五个整数的循环。rdx 从 1 涨到 5。每次循环里 ecx 用作移位量，初值 19，每轮 add $0x3，所以第 i 轮右移量是 19 + 3*(i-1)。右移完 and $0xf 只留最低四位，这就是从 challenge 里抠出来的一小段。

接下来 xor %edx,%eax 把这一段和当前下标 i 异或，然后 lea (%rax,%rsi,2) 相当于 rax + 2*rsi，这里的 rsi 是上一轮已经验证过的整数。所以每一项 = (4 位片段 ^ i) + 2 * 前一项。

2 乘以任意 32 位数可能溢出，但汇编用的是 32 位寄存器，C 里也应该用 uint32_t，让它自然取模。

3. 计算关系

v[0] = (uint32_t)((challenge >> 41) & 0xFF) ^ 7;
for (int i = 1; i <= 5; i++) {
    uint32_t shift = 19 + 3 * (i - 1);
    uint32_t bits = (uint32_t)((challenge >> shift) & 0xF);
    v[i] = (bits ^ (uint32_t)i) + 2 * v[i - 1];
}

4. 验证

./defuser --seed 43 --phase 2 | ./bomb --seed 43 --phase 2
输出 Phase 2 defused.

截图：images/phase2.png

（四）第三关-----------------------------------------------------------

1. 关键汇编

phase_03：

  40223d:	mov    $0x2,%esi               ; 需要 2 个整数
  40224a:	call   valid_u32_fields
  402272:	call   __isoc99_sscanf@plt
  402277:	cmp    $0x2,%eax
  40227c:	mov    %r12d,%edi
  402281:	and    $0x7,%edi                ; v0 = challenge & 7
  402284:	cmp    %edi,-0x18(%rbp)
  402289:	mov    %r12,%rax
  40228c:	mov    %r12,%rsi
  40228f:	shr    $0x3,%rax                ; challenge >> 3
  402293:	shr    $0x20,%rsi               ; challenge >> 32
  402297:	xor    %eax,%esi                 ; X
  402299:	call   p3_dispatch
  40229e:	cmp    %eax,-0x14(%rbp)         ; 第 2 个输入 == 返回值？

p3_dispatch：

  4021d4:	mov    %edi,%eax
  4021d6:	mov    %esi,%edi
  4021d8:	cmp    $0x7,%eax
  4021db:	ja     402225
  4021dd:	notrack jmp *0x4057e0(,%rax,8)

2. 推理过程

前面 sscanf 返回值必须等于 2，所以这一关两个整数。

第一个整数直接用 challenge & 7 得到，然后和输入比。第二个整数需要调用 p3_dispatch 计算出来。调用前 rdi 是 v0，rsi 是 X = (challenge >> 32) ^ (challenge >> 3)。p3_dispatch 内部会把这两个参数对调，再按 v0 查跳转表跳到某个 p3_case_N。

跳转表在 0x4057e0，8 个 8 字节的地址。用 GDB 读出来是：

  0x4057e0: 0x4021f8  0x402200  0x402208  0x402210
  0x405800: 0x402218  0x402220  0x4021e8  0x4021f0

这 8 个地址其实是 8 段跳板代码，每段再跳到真正的 p3_case_N。对照地址表可以查出函数名。写这份报告时我第一次猜的顺序是错的，导致炸弹爆炸，后来在 GDB 里把表读出来才修正。

映射结果是：

  v0 = 0 调用 p3_case_4, 公式 (2*X) ^ 0xbbe1
  v0 = 1 调用 p3_case_1, 公式 (4*X) ^ 0x9ed7
  v0 = 2 调用 p3_case_7, 公式 (X ^ 0x5044) + (X >> 2)
  v0 = 3 调用 p3_case_6, 公式 5*X + 0x2855
  v0 = 4 调用 p3_case_5, 公式 5*X + 0xc243
  v0 = 5 调用 p3_case_3, 公式 (X ^ 0xee13) + (X >> 1)
  v0 = 6 调用 p3_case_2, 公式 3*X + 0x1931
  v0 = 7 调用 p3_case_0, 公式 5*X + 0x3d20c

每个 case 都只是一两条 lea/xor/add，直接按汇编翻译即可，用 uint32_t 保证 32 位回绕。

3. 计算关系

v0 = challenge & 7;
X  = (uint32_t)(challenge >> 32) ^ (uint32_t)(challenge >> 3);
用上面的表根据 v0 选公式算 v1。

4. 验证

./defuser --seed 43 --phase 3 | ./bomb --seed 43 --phase 3
输出 Phase 3 defused.

截图：images/phase3.png

（五）第四关---------------------------------------------------------------

1. 关键汇编

phase_04：

  40232d:	mov    $0x1,%esi
  40233a:	call   valid_u32_fields
  40235e:	call   __isoc99_sscanf@plt
  402363:	cmp    $0x1,%eax
  402368:	mov    %r12,%rcx               ; rcx = challenge
  40236e:	mov    %r12d,%edi
  402371:	xor    %esi,%esi                ; lo = 0
  402373:	shr    $0x20,%rax               ; challenge >> 32
  402377:	shr    $0x7,%rcx                ; challenge >> 7
  40237b:	and    $0x7f,%edi                ; target = challenge & 0x7f
  40237e:	mov    $0x7f,%edx                ; hi = 0x7f
  402383:	xor    %eax,%ecx                ; key
  402385:	call   p4_walk

p4_walk：

  4022b4:	mov    %edi,%eax               ; eax = target
  4022b6:	mov    %edx,%edi               ; edi = hi
  4022b8:	sub    %esi,%edi               ; hi - lo
  4022ba:	shr    $1,%edi                  ; /2
  4022bc:	add    %esi,%edi               ; mid = lo + (hi-lo)/2
  4022be:	cmp    %eax,%edi
  4022c0:	je     402310                  ; mid == target
  4022c7:	lea    (%rdi,%rcx,1),%ebx      ; ebx = mid + key
  4022ce:	cmp    %edi,%eax
  4022d0:	jb     4022f0                  ; target < mid
  4022d2:	lea    0x1(%rdi),%esi          ; 新 lo = mid + 1
  4022d5:	mov    %eax,%edi
  4022d7:	call   p4_walk
  4022dc:	lea    (%rax,%rax,4),%eax       ; *5
  4022df:	xor    %ebx,%eax
  4022e6:	xor    $0xb,%eax
  4022e9:	ret
  4022f0:	lea    -0x1(%rdi),%edx          ; 新 hi = mid - 1
  4022f3:	mov    %eax,%edi
  4022f5:	call   p4_walk
  4022fa:	lea    (%rax,%rax,4),%eax
  4022fd:	xor    %ebx,%eax
  402304:	xor    $0x15,%eax
  402307:	ret
  402310:	lea    0x16(%rdi),%eax
  402313:	xor    %ecx,%eax
  402315:	ret

2. 推理过程

sscanf 返回 1，需要 1 个整数。

入口参数：edi = challenge & 0x7f，esi = 0，edx = 0x7f，ecx = (challenge >> 7) ^ (challenge >> 32)。也就是说用 target、lo=0、hi=127、key 四个参数调用 p4_walk。

p4_walk 里第一件事是计算 mid = lo + (hi - lo) / 2，这是典型的二分查找取中点。然后把 mid 和 target 比：

如果 mid == target，进入基础情况。基础情况返回 (mid + 0x16) ^ key。

如果 target < mid，走左分支：新 hi = mid - 1，递归进去，返回后做 (r * 5) ^ (mid + key) ^ 0x15。

如果 target > mid，走右分支：新 lo = mid + 1，递归进去，返回后做 (r * 5) ^ (mid + key) ^ 0xb。

反汇编里 lea (%rdi,%rcx,1),%ebx 就是把 mid + key 先存到 ebx，递归返回后再异或，这个 ebx 相当于一个临时变量，递归前后不变。

递归深度最多 7 层，因为区间是 0 到 127。写 C 的时候可以直接用递归，把 ebx 作为局部变量保存。

3. 计算关系

uint32_t p4_walk(uint32_t target, uint32_t lo, uint32_t hi, uint32_t key) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (mid == target)
        return (mid + 0x16) ^ key;
    uint32_t ebx = mid + key;
    if (target < mid) {
        uint32_t r = p4_walk(target, lo, mid - 1, key);
        return (r * 5) ^ ebx ^ 0x15;
    } else {
        uint32_t r = p4_walk(target, mid + 1, hi, key);
        return (r * 5) ^ ebx ^ 0xb;
    }
}

target = (uint32_t)challenge & 0x7f;
key = (uint32_t)(challenge >> 7) ^ (uint32_t)(challenge >> 32);
answer = p4_walk(target, 0, 0x7f, key);

4. 验证

./defuser --seed 43 --phase 4 | ./bomb --seed 43 --phase 4
输出 Phase 4 defused.

截图：images/phase4.png

（六）第五关--------------------------------------------------------------------

1. 数据结构

p5_heads 在 0x405b20，8 个链表头。每个节点 16 字节：偏移 0 是 field0（4 字节），偏移 4 是 field1（4 字节），偏移 8 是 next 指针（8 字节）。

用 GDB 逐个读每个链表头的节点内容，整理如下：

  编号 0:  field0=0x46dd field1=0x164 next=5
  编号 1:  field0=0xb916 field1=0x826 next=4
  编号 2:  field0=0xe72a field1=0x529 next=8
  编号 3:  field0=0x653f field1=0x27e next=7
  编号 4:  field0=0x1367 field1=0x414 next=9
  编号 5:  field0=0x2d23 field1=0x331 next=11
  编号 6:  field0=0x9d47 field1=0xa6d next=2
  编号 7:  field0=0x186e field1=0x022 next=1
  编号 8:  field0=0x3720 field1=0xb17 next=10
  编号 9:  field0=0x85ad field1=0x767 next=0
  编号 10: field0=0x5c9c field1=0x956 next=NULL
  编号 11: field0=0xddfe field1=0x630 next=6

链表头数组是 {3, 7, 1, 4, 9, 0, 5, 11}。

2. 关键汇编

p5_traverse：

  4023ad:	and    $0x7,%eax                ; idx = challenge & 7
  4023b4:	mov    0x405b20(,%rax,8),%rdx  ; node = p5_heads[idx]
  4023bf:	shr    $0x20,%rdi               ; challenge >> 32
  4023c3:	shr    $0x6,%rax                ; challenge >> 6
  4023c7:	xor    %edi,%eax                ; (challenge>>6) ^ (challenge>>32)
  4023c9:	mov    0x4(%rdx),%esi           ; field1
  4023ce:	xor    $0x14b7,%eax             ; ^ 0x14b7
  4023d3:	add    (%rdx),%eax             ; += field0
  4023d5:	lea    (%rax,%rax,4),%eax        ; *= 5
  4023d8:	mov    %esi,(%r8)              ; 写回 field1
  4023db:	xor    %esi,%eax                ; ^= field1
  4023dd:	and    $0x3,%edi                ; steps = (challenge>>3) & 3
  4023e0:	je     4023ff
  4023e7:	mov    0x8(%rdx),%rdx           ; node = node->next
  4023ee:	mov    0x4(%rdx),%esi
  4023f1:	add    (%rdx),%eax
  4023f3:	lea    (%rax,%rax,4),%eax
  4023f6:	mov    %esi,(%r8)
  4023f9:	xor    %esi,%eax
  4023fb:	cmp    %ecx,%edi
  4023fd:	jae    4023e7

phase_05 里两次比较：

  402453:	call   p5_traverse
  40245c:	cmp    %edx,-0x1c(%rbp)          ; 第 1 个输入 == 返回值？
  402461:	mov    -0x14(%rbp),%eax
  402464:	cmp    %eax,-0x18(%rbp)          ; 第 2 个输入 == 写回值？

3. 推理过程

入口 esi = 2 且 sscanf 返回值必须为 2，所以这一关两个整数。

从 phase_05 可见 p5_traverse 返回一个值（在 eax），同时通过第二个参数（rsi，也就是 -0x14(%rbp)）写回一个值。第 1 个输入要等于返回值，第 2 个输入要等于写回值。

看 p5_traverse 内部：

先用 challenge & 7 取 0 到 7 的索引，在 p5_heads 里选一条链表。

初始化 eax = ((challenge >> 6) ^ (challenge >> 32)) ^ 0x14b7。

对当前节点做三件事：eax += field0，eax *= 5，eax ^= field1。同时把当前节点的 field1 通过 r8 写入调用者提供的缓冲区。

接下来根据 (challenge >> 3) & 3 决定沿 next 走几步。0 步的话直接结束，否则用 ecx 从 1 开始计数，每走一个节点重复上面那三步，并把新的 field1 覆盖写入缓冲区。

所以缓冲区里最终留下的是最后一个访问节点的 field1，而 eax 是所有经过的节点累积运算后的结果。

索引和步数都是小范围值（0..7 和 0..3），链表长度不超过 4，可以直接用查表实现。

4. 计算关系

idx = challenge & 7;
steps = (challenge >> 3) & 3;
node = p5_heads[idx];

eax = ((uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32)) ^ 0x14b7;
eax += field0(node); eax *= 5;
last_field1 = field1(node);
eax ^= last_field1;

for (i = 0; i < steps; i++) {
    node = next(node);
    eax += field0(node); eax *= 5;
    last_field1 = field1(node);
    eax ^= last_field1;
}

第一个输入 = eax，第二个输入 = last_field1。

5. 验证

./defuser --seed 43 --phase 5 | ./bomb --seed 43 --phase 5
输出 Phase 5 defused.

截图：images/phase5.png

（七）第六关------------------------------------------------------------

1. 关键汇编

p6_dispatch：

  402509:	mov    %rdi,%rbx
  40250c:	shr    $0x6,%rdi                ; challenge >> 6
  402513:	shr    $0x20,%rax               ; challenge >> 32
  402517:	xor    %eax,%edi                ; arg1 = (challenge>>6) ^ (challenge>>32)
  402520:	and    $0x38,%ebx               ; (challenge & 0x38) = idx2 * 8
  402523:	and    $0x7,%eax                ; idx1 = challenge & 7
  402526:	call   *0x405dc0(,%rax,8)       ; 第一次调用
  40252d:	xor    $0xb2c1,%eax
  402532:	mov    %eax,%edi
  402534:	call   *0x405dc0(%rbx)          ; 第二次调用
  40253f:	add    $0xdba7,%eax

phase_06 最后：

  40259b:	call   p6_dispatch
  4025a0:	cmp    %eax,-0x14(%rbp)          ; 输入 == 返回值？

2. 推理过程

sscanf 返回 1，需要 1 个整数。

p6_dispatch 把 challenge 拆成几部分。idx1 是低 3 位，用作函数指针表的索引。arg1 是 (challenge >> 6) ^ (challenge >> 32)，作为第一次调用的参数。idx2 是 (challenge >> 3) & 7，用 challenge & 0x38 等价表示（因为表元素 8 字节）。

第一次调用的返回值先异或 0xb2c1，作为第二次调用的参数。第二次调用结束后再加 0xdba7，就是 p6_dispatch 的返回值。

函数指针表在 0x405dc0，8 项。用 GDB 读出来是：

  0x405dc0: 0x402470  0x402470  0x4024e0  0x4024d0
  0x405de0: 0x4024e0  0x4024f0  0x402490  0x4024b0

对照 info functions p6_op 给出地址，映射如下：

  idx 0 -> p6_op_0
  idx 1 -> p6_op_0
  idx 2 -> p6_op_4
  idx 3 -> p6_op_3
  idx 4 -> p6_op_4
  idx 5 -> p6_op_5
  idx 6 -> p6_op_1
  idx 7 -> p6_op_2

每个操作函数都很短，直接读汇编翻译：

p6_op_0 先 shr $0x7 再 xor 原值，得到 (x >> 7) ^ x，再加 0x49c2。
p6_op_1 先把 x << 7，再算 x ^ 0x571d，两者相加。
p6_op_2 是 8*x + 0x94c3 和 3*x 异或。
p6_op_3 循环左移 1 位，再异或 0x88b3。
p6_op_4 循环右移 1 位。
p6_op_5 是 (x + 0xa8e8) 再异或 (x >> 5)。

循环移位在 32 位下要用 (x << 1) | (x >> 31) 和 (x >> 1) | (x << 31)，不能用 n = 32 的左移（那是未定义行为）。

3. 计算关系

idx1 = challenge & 7;
arg1 = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32);
r1 = table[idx1](arg1);
idx2 = (challenge >> 3) & 7;
arg2 = r1 ^ 0xb2c1;
r2 = table[idx2](arg2);
answer = r2 + 0xdba7;

4. 验证

./defuser --seed 43 --phase 6 | ./bomb --seed 43 --phase 6
输出 Phase 6 defused.

截图：images/phase6.png

（八）Honor（Secret Phase）----------------------------------------------

1. 输入格式

secret_gate 会检查输入字符串。先看长度：

  4026ec:	call   strlen@plt
  4026f1:	sub    $0x24,%rax                ; strlen(input) - 36
  4026f5:	cmp    $0x7f,%rax
  4026f9:	jbe    402710                    ; 必须 36 <= len <= 36 + 127

然后检查第 32~35 字节是不是 "OPEN"（小端 0x4e45504f）：

  40271b:	cmpl   $0x4e45504f,-0xb0(%rbp)

通过了之后 secret_gate 把 input 指针和 challenge、context 一起传给 secret_phase。

2. 关键汇编

secret_phase：

  4025da:	mov    %rdx,%r13                ; r13 = context
  4025df:	mov    %rsi,%r12                ; r12 = challenge
  4025e2:	mov    $0x3,%esi               ; 需要 3 个整数
  4025ef:	call   valid_u32_fields
  40261a:	mov    $0x40317c,%esi          ; 格式串 "%u %u %u"
  402626:	call   __isoc99_sscanf@plt
  40262b:	cmp    $0x3,%eax
  402630:	mov    %r13d,%ecx
  402635:	and    $0x3,%ecx               ; a = context & 3
  402638:	cmp    %ecx,-0x2c(%rbp)         ; 第 1 个输入 == a？
  40263d:	mov    %r12,%rsi
  402640:	imul   $0xb07a,%ecx,%r9d        ; r9d = 0xb07a * a
  402647:	mov    %r13,%r8
  40264a:	shr    $0x20,%rsi               ; challenge >> 32
  40264e:	shr    $0x20,%r8                ; context >> 32
  402652:	xor    %r12d,%esi               ; X = (challenge>>32) ^ challenge
  402655:	xor    %r13d,%r8d               ; Y = (context>>32) ^ context
  402658:	shr    $0x10,%r13               ; context >> 16
  40265c:	mov    %esi,%edi
  40265e:	xor    %r13d,%r9d               ; Z = 0xb07a*a ^ (context>>16)
  402661:	xor    %r8d,%edi                ; t = X ^ Y
  402664:	call   secret_mix
  402669:	xor    %r9d,%eax                ; b = mix ^ Z
  40266c:	cmp    %eax,-0x28(%rbp)         ; 第 2 个输入 == b？
  402671:	shr    $1,%esi                 ; X >> 1
  402673:	lea    0x3b81(%r8,%rcx,1),%edx  ; 0x3b81 + Y + a
  40267b:	add    %esi,%edx                ; + (X >> 1)
  40267d:	xor    %edx,%eax                ; c = b ^ ...
  40267f:	cmp    -0x24(%rbp),%eax         ; 第 3 个输入 == c？
  402682:	sete   %dl

secret_mix：

  4025b4:	mov    %edi,%eax
  4025b6:	rol    $1,%eax                   ; 循环左移 1 位
  4025b8:	add    $0xb07a,%eax
  4025bd:	xor    $0x3b81,%eax

3. 推理过程

secret_gate 先把输入复制到缓冲区，检查第 32~35 字节是否等于 "OPEN"。通过后把 input 指针、challenge、context 传给 secret_phase。

secret_phase 里 sscanf 使用 "%u %u %u" 格式，会自动跳过前面的非数字字符直到遇到第一个数字。这就是为什么前 32 字节可以随便填（比如全填 X），只要不含数字、至少 32 字节长就行。

然后依次校验三个整数：

第 1 个等于 a = context & 3。

第 2 个等于 b。b 的推导：先把 challenge 拆成高 32 位和低 32 位异或，得到 X = (challenge >> 32) ^ challenge。context 同理得到 Y = (context >> 32) ^ context。a 乘 0xb07a 再异或 (context >> 16) 得到 Z。然后 t = X ^ Y，把 t 传给 secret_mix：先 rol 1 位，加 0xb07a，再异或 0x3b81，得到 mix。b = mix ^ Z。

第 3 个等于 c。c = b ^ (0x3b81 + Y + a + (X >> 1))。

最后输出构造：前 32 字节填任意非空字符，紧跟 "OPEN"，再接 "a b c"。

4. 计算关系

a = context & 3;
X = (uint32_t)(challenge >> 32) ^ (uint32_t)challenge;
Y = (uint32_t)(context >> 32) ^ (uint32_t)context;
Z = (uint32_t)(0xb07a * a) ^ (uint32_t)(context >> 16);

t = X ^ Y;
mix = ((t << 1) | (t >> 31)) + 0xb07a;
mix ^= 0x3b81;

b = mix ^ Z;
c = b ^ (0x3b81 + Y + a + (X >> 1));

输出：32 个 'X' + "OPEN" + "a b c"。

5. 验证

{ ./defuser --seed 43; ./defuser --secret --seed 43; } | ./bomb --seed 43
输出 Secret phase defused.

截图：images/secret.png

（九）公开测试结果------------------------------------------------------

Public tests: 60/60
Secret public tests: 10/10

截图：images/all_tests.png

（十）引用资料与工具---------------------------------------------------------

objdump：反汇编 bomb，导出各关函数和辅助函数。
GDB：查看跳转表、函数指针表、链表数据，并在线验证公式。
CS:APP 第 3 章：x86-64 汇编、寄存器约定、函数调用。
课程主页 BombLab 文档：实验流程与提交要求。
AI 辅助工具：用于帮助理解汇编指令、整理报告结构；所有计算关系均在 GDB 中验证过。