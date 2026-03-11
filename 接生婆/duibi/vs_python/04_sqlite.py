import sqlite3

print("【Python】SQLite 操作（需 import sqlite3）：")

con = sqlite3.connect(":memory:")
cur = con.cursor()

cur.execute("CREATE TABLE 卦象 (id INTEGER PRIMARY KEY, 名 TEXT, 象 TEXT, 数 INTEGER)")
data = [(1,"乾","天",1),(2,"坤","地",2),(3,"震","雷",51),
        (4,"巽","风",57),(5,"坎","水",29),(6,"离","火",30)]
cur.executemany("INSERT INTO 卦象 VALUES (?,?,?,?)", data)

for row in cur.execute("SELECT 名,象,数 FROM 卦象 ORDER BY 数"):
    print(row)

for row in cur.execute("SELECT COUNT(*) as 总数, AVG(数) as 均值 FROM 卦象"):
    print(row)

con.close()
print("数据库已关闭")
