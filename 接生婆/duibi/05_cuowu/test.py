x = 5

try:
    assert x > 10, f"断言失败：x={x} 不满足 > 10"
    print("断言通过")
except AssertionError as e:
    print("捕获:", e)
