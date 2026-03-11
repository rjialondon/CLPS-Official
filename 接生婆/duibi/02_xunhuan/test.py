# 小规模：求和 range(5)
total = 0
for i in range(5):
    total += i
print(total)          # 0+1+2+3+4 = 10

# 大规模：求和 range(1000)
total2 = 0
for i in range(1000):
    total2 += i
print(total2)         # 499500
