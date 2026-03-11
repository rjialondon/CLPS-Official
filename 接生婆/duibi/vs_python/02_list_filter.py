import random

print("【Python】列表过滤求和：")

nums = [random.randint(0, 99) for _ in range(20)]
big  = [x for x in nums if x > 50]

print("全数：", nums)
print("大数：", big)
print(f"大于50的个数：{len(big)}，总和：{sum(big)}")
