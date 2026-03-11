import time

# 显式 for 循环，与 CLPS 复 N 对等
t0 = time.time()
s = 0
for i in range(100000):
    s += i
t1 = time.time()
print(f"sum={s}  耗时={int((t1-t0)*1000)} ms")
