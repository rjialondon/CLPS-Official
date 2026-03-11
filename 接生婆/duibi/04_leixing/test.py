# Python：动态类型，编译器不检查，运行时才报错
x = 42
try:
    x = x + "hello"      # TypeError: unsupported operand type(s)
    print("成功")
except TypeError as e:
    print("运行时错误:", e)
print("x =", x)          # x 未变，仍是 42
