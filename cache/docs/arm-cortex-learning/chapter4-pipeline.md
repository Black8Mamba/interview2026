# 第4章 流水线技术

> **📖 基础层** | **⚙️ 原理层** | **🔧 实战层**

---

## 📖 基础层：流水线概念

### 什么是流水线

流水线是一种将指令执行分解为多个阶段的技术，使得多条指令可以并行执行（重叠执行），提高处理器吞吐量。

**非流水线 vs 流水线：**
```
非流水线：
┌─────┬─────┬─────┬─────┬─────┐
│ I1  │ I2  │ I3  │ I4  │ I5  │
└─────┴─────┴─────┴─────┴─────┘

流水线（理想情况）：
┌─────┬─────┬─────┬─────┬─────┐
│ I1  │ I2  │ I3  │ I4  │ I5  │
├─────┼─────┼─────┼─────┼─────┤
│     │ I1  │ I2  │ I3  │ I4  │
├─────┼─────┼─────┼─────┼─────┤
│     │     │ I1  │ I2  │ I3  │
├─────┼─────┼─────┼─────┼─────┤
│     │     │     │ I1  │ I2  │
└─────┴─────┴─────┴─────┴─────┘
      F   →   E   →   M   →   W
```

### 五级流水线

经典的RISC五级流水线：

| 阶段 | 名称 | 功能 |
|------|------|------|
| **F** | Fetch | 取指：从指令缓存/内存读取指令 |
| **D** | Decode | 译码：解析指令、操作数 |
| **E** | Execute | 执行：算术逻辑运算 |
| **M** | Memory | 访存：访问数据缓存/内存 |
| **W** | Writeback | 写回：结果写回寄存器 |

> **术语对照**
> - Pipeline: 流水线
> - Throughput: 吞吐量
> - Latency: 延迟
> - IPC: Instructions Per Cycle，每周期指令数

---

## ⚙️ 原理层：高级流水线技术

### 超标量（Superscalar）

超标量处理器在每个时钟周期可以发射多条指令到多个执行单元：

```
标量处理器（每周期1条）：
Cycle:  1   2   3   4   5
        I1  I2  I3  I4  I5

超标量处理器（每周期2条）：
Cycle:  1   2   3   4   5
        I1  I2  I3  I4  I5
        I1' I2' I3' I4' I5'
```

**Arm Cortex-A系列超标量能力：**
| 微架构 | 发射宽度 | 执行单元 |
|--------|----------|----------|
| A55 | 2路 | 2 ALU + 1 FP |
| A76 | 4路 | 4 ALU + 2 FP + 1 SIMD |
| X1/X2 | 5路 | 5 ALU + 2 FP + 2 SIMD |

### 乱序执行（Out-of-Order）

当指令存在数据依赖时，处理器可以动态调度后续不依赖的指令先执行：

```c
// 示例代码
I1: ADD x1, x2, x3    // x1 = x2 + x3
I2: SUB x4, x1, x5    // x4 = x1 - x5  ← 依赖I1
I3: MUL x6, x7, x8    // x6 = x7 * x8  ← 独立，可提前执行

// 顺序执行：
//   I1 → I2 → I3

// 乱序执行：
//   I1 → (I3提前) → I2
```

**乱序执行的关键组件：**
```
┌─────────────────────────────────────────────────────┐
│                  乱序执行引擎                         │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐      │
│  │  Dispatch│   │  Rename   │   │  ROB     │      │
│  │  (分发)   │ → │  (寄存器重命名)│ → │( reorder)│      │
│  └──────────┘   └──────────┘   └──────────┘      │
│       │              │               │            │
│       ▼              ▼               ▼            │
│  ┌──────────────────────────────────────────┐   │
│  │         Reservation Station (预约站)       │   │
│  │   等待操作数就绪后发射到执行单元             │   │
│  └──────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

### 分支预测

分支预测减少控制冒险带来的流水线停顿：

**1. BTB (Branch Target Buffer)**
```c
// BTB存储分支目标地址
// 预测命中 → 直接从BTB获取目标地址
struct BTB_entry {
    uint64_t pc;        // 分支指令地址
    uint64_t target;    // 目标地址
    uint8_t confidence; // 置信度
};
```

**2. BHT (Branch History Table)**
```c
// BHT基于历史记录预测分支方向
// 2位饱和计数器：
//   11: Strongly Taken
//   10: Weakly Taken
//   01: Weakly Not Taken
//   00: Strongly Not Taken
```

**3. RAS (Return Address Stack)**
```c
// 函数调用/返回时操作RAS
// CALL: push(return_address) → RAS
// RET:  pop() → 预测返回地址

function call() {
    // 压栈返回地址
    ras.push(pc + 4);
    goto function;
}

function return_() {
    // 弹栈预测返回
    pc = ras.pop();
}
```

### 流水线冒险（Hazard）

**1. 数据冒险**
```c
// RAW (Read After Write) - 真数据依赖
I1: ADD x1, x2, x3    // x1 = x2 + x3
I2: SUB x4, x1, x5    // x4 = x1 - x5  ← 需等待I1结果

// WAR (Write After Read) - 反依赖
I1: SUB x1, x4, x5    // x1 = x4 - x5
I2: ADD x4, x2, x3    // x4 = x2 + x3  ← 不能先于I1

// WAW (Write After Write) - 输出依赖
I1: ADD x1, x2, x3    // x1 = x2 + x3
I2: SUB x1, x4, x5    // x1 = x4 - x5  ← 需按序完成
```

**2. 控制冒险**
```c
// 分支指令导致流水线清空
    ADD x1, x2, x3
    B  target        // 分支
    SUB x4, x5, x6  // 这条可能被错误执行
target:
    MUL x7, x8, x9
```

**3. 结构冒险**
```c
// 硬件资源不足
// 例如：只有一个乘法器，两条乘法指令无法并行
I1: MUL x1, x2, x3
I2: MUL x4, x5, x6  // 需等待I1完成
```

**解决方案：**
- 转发（Forwarding）：直接从执行单元传递结果给后续指令
- stall（停顿）：插入空泡等待数据就绪
- 分支延迟槽（已较少使用）
- 预测执行：猜测执行，错误时回滚

> **术语对照**
> - Superscalar: 超标量
> - Out-of-Order: 乱序执行
> - BTB: Branch Target Buffer，分支目标缓冲
> - BHT: Branch History Table，分支历史表
> - RAS: Return Address Stack，返回地址栈
> - RAW/WAR/WAW: 数据依赖类型
> - Stall: 停顿

---

## 🔧 实战层：编译器优化与调度

### 循环展开

```c
// 未优化
for (int i = 0; i < N; i++) {
    sum += data[i];
}

// 优化：4次展开
for (int i = 0; i < N; i += 4) {
    sum += data[i];
    sum += data[i+1];
    sum += data[i+2];
    sum += data[i+3];
}
// 减少循环开销，增加指令级并行
```

### 循环合并

```c
// 两个独立循环
for (int i = 0; i < N; i++) {
    a[i] = b[i] * 2;
}
for (int i = 0; i < N; i++) {
    c[i] = a[i] + 1;
}

// 合并为一个循环
for (int i = 0; i < N; i++) {
    a[i] = b[i] * 2;
    c[i] = a[i] + 1;
}
// 增加数据局部性，减少内存访问
```

### 分支优化

```c
// 减少分支预测失败惩罚

// 1. likely/unlikely提示
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

if (unlikely(error)) {
    handle_error();   // 很少执行的分支
}

// 2. 用条件指令替代分支
// 原来：
int result;
if (a > b) {
    result = a;
} else {
    result = b;
}

// 优化：Csel指令
result = (a > b) ? a : b;  // 编译为 CSEL x0, x1, x2, GT
```

### 内存访问优化

```c
// 1. 预取
for (int i = 0; i < N; i++) {
    __builtin_prefetch(&data[i + PREFETCH_DIST], 0, 3);
    process(data[i]);
}

// 2. 数据对齐
struct __attribute__((aligned(64))) aligned_data {
    // 数据对齐到缓存行
};

// 3. 避免跨缓存行访问
// 合理安排数据结构
```

### Arm64intrinsic优化

```c
#include <arm_neon.h>

// 向量化加法
float32x4_t vector_add(float32x4_t a, float32x4_t b) {
    return vaddq_f32(a, b);  // 单指令完成4个float加法
}

// 矩阵乘法优化
void matmul(float *a, float *b, float *c, int n) {
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < n; k++) {
            float aik = a[i*n + k];
            for (int j = 0; j < n; j += 4) {
                float32x4_t cij = vld1q_f32(&c[i*n + j]);
                float32x4_t bkj = vld1q_f32(&b[k*n + j]);
                cij = vmlaq_f32(cij, vdupq_n_f32(aik), bkj);
                vst1q_f32(&c[i*n + j], cij);
            }
        }
    }
}
```

---

## 本章小结

- 流水线通过指令重叠执行提高吞吐量
- 超标量技术实现多指令并行发射
- 乱序执行动态调度无关指令
- 分支预测减少控制冒险
- 数据冒险通过转发和寄存器重命名解决
- 编译器优化：循环展开、预取、SIMD向量化

---

**下一章**：我们将学习 [内存模型](./chapter5-memory-model.md)，了解Arm的内存顺序语义。
