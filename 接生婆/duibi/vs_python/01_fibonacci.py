print("【Python】斐波那契前10项：")
a, b = 0, 1
for i in range(10):
    print(f"F({i}) = {a}")
    a, b = b, a + b
