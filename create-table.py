

def main():
    NFILES = 64
    NODES = 10
    table = []
    for i in range(NFILES):
        with open("out{}.txt".format(i), "r") as f:
            lines = f.readlines()
            found = 0
            n_line = 0
            while found != 2 and n_line < len(lines):
                if "Just the leadership" in lines[n_line]:
                    found += 1
                n_line += 1

            if found == 2:
                column = []
                for j in range(NODES):
                    n = int(lines[n_line+j])
                    column.append(n)
                table.append(column)

    for j in range(NODES):
        for i in range(NFILES):
            print(table[i][j], end=" ")
        print("")
                    
                

main()
