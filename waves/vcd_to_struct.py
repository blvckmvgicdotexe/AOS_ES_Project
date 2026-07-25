import sys

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <vcd file>")
else:
    filename = sys.argv[1];

    print("{", end = "")

    with open(filename, "r") as file:
        f_str = file.read()
        start = f_str.find("#")   # find first useful line
        f_str = f_str[start::]
        lines = f_str.splitlines()

    linenum = 0

    for line in lines:
        linenum = linenum + 1
        pieces = line.split(" ")
        num_str = pieces[0][1::]
        edge_str = pieces[1][0:1:]
        print ("{", end = "")
        if edge_str == "0":
            print ("FALLING_EDGE, ", end = "")
        else:
            print ("RISING_EDGE, ", end = "")

        print (f"{num_str}", end = "")

        print ("}", end = "")

        if linenum != len(lines):
            print (",")




    print("}", end = "")
