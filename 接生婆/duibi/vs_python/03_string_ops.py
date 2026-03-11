print("【Python】字符串操作：")

s = "The Tao that can be named is not the eternal Tao."

print(s.upper())
print(s.lower())
print(f"长度：{len(s)} 字符")
print(f'"Tao" 首次出现于第 {s.index("Tao")} 位')
print(s.replace("Tao", "道"))
print(s[:15])
print(f"共 {len(s.split())} 个词")
