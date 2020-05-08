import os,sys
application_addr = 0x08003000

elf_old_path = "loramac/LoRaMac4.0.0.axf"#"test01"
elf_new_path = "loramac/LoRaMac4.1.0.axf"#"test"

findK_table = []

def trans(bins):
    return ''.join( [ "%02X" % x for x in bins ] ).strip()
    
def bin_modifier(bin_path, rel_path):
    fbin = open(bin_path, 'rb')
    bin_bytes = fbin.read()
    print(bin_path,len(bin_bytes))
    bin_byte_array = bytearray(bin_bytes)
    fbin.close()

    frel = open(rel_path, 'r')
    lines = frel.readlines()
    frel.close()
    reloc_list = []
    for line in lines:
        try:
            addr = int(line[0:8],16)
            if addr>application_addr and addr<0x08030000:
                offset = addr - application_addr
                r = int.from_bytes(bin_bytes[offset:offset+4], byteorder='little', signed=False)
                bin_byte_array[offset] = 0
                bin_byte_array[offset+1] = 0
                bin_byte_array[offset+2] = 0
                bin_byte_array[offset+3] = 0

                reloc_list.append((addr,r, line.split()[-1]))
        except:
            continue
    return (bin_byte_array, reloc_list)


#new[k,i-1] matchs segment in old[off:end_addr-1]
def findK(old, new, i):
    min_k = i
    off = -1
    j=len(old)
    while j>=0:
        if old[j-1]==new[i-1]:
            k = i - 1
            while k>=max(i-j,0):
                if old[j+k-i]==new[k]:
                    k-=1
                else:
                    break
            if k + 1 < min_k:
                min_k = k + 1
                off = j + k - i + 1
        j-=1
    return (min_k, off)


def find_diff(old, new):
    # copy: (cost, 0, n, addr)
    # add: (cost, 1, n)
    # type 1bit  n 3bytes  addr 3bytes  copycost:6 addcost:n+3 
    opt = [(0,1,0,0)for i in range(len(new)+1)]
    for i in range(1, len(new)+1):
        if opt[i-1][1]==0:
            if opt[i-1][2]+opt[i-1][3] <len(old):
                if old[opt[i-1][2]+opt[i-1][3]]==new[i-1]:
                    opt[i] = (opt[i-1][0], 0, opt[i-1][2]+1, opt[i-1][3])
                    continue
            
        #new[k,i-1]==old[]
        (k, offset) = findK(old, new, i)
        if k < i:
            # opt[k] + copy(k,i]
            cost1 = opt[k][0] + 6
            # opt[k] + add[i]
            if opt[i-1][1]==1:
                cost2 = opt[i-1][0] + 1
            else:
                # new add
                cost2 = opt[i-1][0] + 4
            
            if cost1 < cost2:
                opt[i] = (cost1, 0, i-k, offset)
            elif opt[i-1][1]==0:
                opt[i] = (cost2, 1, 1)
            else:
                opt[i] = (cost2, 1, opt[i-1][2]+1)
                
        else:
            cost2 = opt[i-1][0] + 4
            opt[i] = (cost2, 1, 1)

    deltas = []
    i = len(new)
    while i > 0:
        deltas.append(opt[i])
        i-=opt[i][2]
        
    deltas.reverse()
    return deltas

def Myers_diff(old, new):
    m = len(old) + len(new)
    v = (2*m+2)*[0]
    snakes = []
    diff_result = []
    for d in range(m+1):
        for k in range(-d, d+1, 2):
            down =  (k == -d or (k != d and v[k - 1 + m] < v[k + 1 + m]))
            kPrev =  k + 1 if down else k - 1
            
            xStart = v[kPrev + m]
            yStart = xStart - kPrev
            xMid = xStart if down else xStart + 1
            yMid = xMid - k
            xEnd = xMid
            yEnd = yMid
            snake = 0;
            while xEnd < len(old) and yEnd < len(new) and old[xEnd][2] == new[yEnd][2]:
                xEnd+=1
                yEnd+=1
                snake+=1
            
            v[k + m]=xEnd
            snakes.append((xStart, yStart, xEnd, yEnd))
            if xEnd >= len(old) and yEnd >= len(new):
                snakes.reverse()
                current = snakes[0]
                diff_result.append((current[2], current[3]))
                for i in range(1,len(snakes)):
                    tmp = snakes[i]
                    if tmp[2] ==current[0] and tmp[3] ==current[1]:
                        current = tmp
                        diff_result.append((current[2], current[3]))
                        if current[0]==0 and current[1]==0:
                            break;
                diff_result.append((0, 0))            
                diff_result.reverse()
                i = 1
                while i + 1 <len(diff_result):
                    if (diff_result[i-1][0] == diff_result[i][0] and diff_result[i +1][0] - diff_result[i][0]<diff_result[i +1][1] - diff_result[i][1]) or (diff_result[i-1][1] == diff_result[i][1] and diff_result[i +1][1] - diff_result[i][1]<diff_result[i +1][0] - diff_result[i][0]):
                        diff_result.pop(i)
                        continue
                    i += 1
                    
                return diff_result

os.system("readelf " + elf_old_path + " -r > old.rel")
os.system("readelf " + elf_new_path + " -r > new.rel")

os.system("D:\\Keil_v5\\ARM\\ARMCC\\bin\\fromelf --bin --output=old.bin " + elf_old_path)
os.system("D:\\Keil_v5\\ARM\\ARMCC\\bin\\fromelf --bin --output=new.bin " + elf_new_path)
#os.system("arm-none-eabi-objcopy -Obinary " + elf_old_path + " old.bin")
# os.system("arm-none-eabi-objcopy -Obinary " + elf_new_path + " new.bin")

(old_bin_modified, old_reloc_list) = bin_modifier("old.bin", "old.rel")
(new_bin_modified, new_reloc_list) = bin_modifier("new.bin", "new.rel") 


reloc_diff_result = Myers_diff(old_reloc_list,new_reloc_list)  

reloc_delta = []  # [start,n,x_off, y_off]
add_reloc = []    # [addr, data]
reloc_max_save_len = 0
for i in range(1,len(reloc_diff_result)):
    l = min(reloc_diff_result[i][0]-reloc_diff_result[i-1][0],reloc_diff_result[i][1]-reloc_diff_result[i-1][1])
    if l==0:
        continue

    start  = (reloc_diff_result[i][0] - l, reloc_diff_result[i][1] - l)
    end = reloc_diff_result[i]
    reloc_max_save_len = max(reloc_max_save_len,reloc_diff_result[i][1] - reloc_diff_result[i][0]) 
    
    for j in range(reloc_diff_result[i-1][1], start[1]):
        add_reloc.append([new_reloc_list[j][0],new_reloc_list[j][1]])
    
    o = old_reloc_list[start[0]]
    n = new_reloc_list[start[1]]
    last = [start[0],1,(n[0]-o[0], n[1]-o[1])]
    for j in range(1,l):
        o = old_reloc_list[start[0]+j]
        n = new_reloc_list[start[1]+j]
        off = (n[0]-o[0], n[1]-o[1])
        if off==last[2]:
            last[1]+=1
        else:
            reloc_delta.append(last)
            last = [start[0]+j,1,off]

    reloc_delta.append(last)
print("reloc_max_save_len",reloc_max_save_len)



count = 0
off_list = []  #[(x_off,y_off),[(start,n)]]
pos = []
for i in reloc_delta:
    #print(i)
    if i[2] in off_list:
        index = off_list.index(i[2])
        pos[index].append((i[0],i[1]))
    else:
        off_list.append(i[2])
        pos.append([(i[0],i[1])])
    count+=i[1]
for i in range(len(off_list)):
    off_list[i] = [off_list[i], pos[i]]
    print(off_list[i])
    
for l in off_list:
    if len(l[1])==1 and l[1][0][1]==1:
        off_list.remove(l)
        add_reloc.append([old_reloc_list[l[1][0][0]][0]+l[0][0],old_reloc_list[l[1][0][0]][1]+l[0][1]])

add_reloc.sort(key=lambda s: s[0])    
off_list.sort(key=lambda s: s[0][0])

for i in add_reloc:
    print("%x" % i[0],"%x" % i[1])

merged_off_list=[]
off_list_data=[]
for i in off_list:
    if i[0][0] in merged_off_list:
        index=merged_off_list.index(i[0][0])
        off_list_data[index].append((i[0][1],i[1]))
    else:
        merged_off_list.append(i[0][0])
        off_list_data.append([(i[0][1],i[1])])
for i in range(len(merged_off_list)):
    merged_off_list[i] = [merged_off_list[i], off_list_data[i]]
    print(merged_off_list[i])    
    
print("off list")    
print(len(old_reloc_list),count, len(add_reloc), len(new_reloc_list))


    
fold_bin = open("old.bin", 'rb')
old_bin = fold_bin.read()
fold_bin.close()

fnew_bin = open("new.bin", 'rb')
new_bin = fnew_bin.read()
fnew_bin.close()


print("deltas compute start")
# deltas = find_diff(old_bin, new_bin)
deltas = find_diff(old_bin_modified, new_bin_modified)
print("deltas compute end")


count_len=0
for i in range(len(deltas)):
    count_len+=deltas[i][2]
    print(count_len, deltas[i])
print(deltas)

prepare_move_table=[]
move_len = 0
max_move = 0
computed_bin = bytearray()
n = 0
add_len = 0
copy_len=0
for delta in deltas:
    part = bytearray(delta[2])
    if delta[1] == 0:
        part = old_bin[delta[3]:delta[3] + delta[2]]
        if n > delta[3]:
            prepare_move_table.append((n, delta))
            move_len += delta[2]
            max_move = max(max_move, n - delta[3])
    else:
        part = new_bin_modified[n:n + delta[2]]
    computed_bin.extend(part)   
    n += delta[2]

print(move_len, max_move)

for reloc in new_reloc_list:
    computed_bin[reloc[0]-application_addr:reloc[0]-application_addr+4]=reloc[1].to_bytes(4, byteorder='little', signed=False)

fix_to_zero_table=[]
for i in range(len(new_bin)):
    if computed_bin[i]!=new_bin[i]:
        fix_to_zero_table.append(i)

print("fix_to_zero_table", fix_to_zero_table)

result = bytearray()
# file struct
header = bytearray(34)
# deltas: type 1bit  n 3bytes  addr 3bytes  copycost:6 addcost:n+3 
r_deltas = bytearray()
n = 0
for delta in deltas:
    r_deltas.extend(((delta[1]<<23)|delta[2]).to_bytes(3, byteorder='little', signed=False))
    if delta[1] == 0: 
        r_deltas.extend(((delta[3]+application_addr)&0x00ffffff).to_bytes(3, byteorder='little', signed=False))
        copy_len+=7
    else:
        r_deltas.extend(new_bin_modified[n:n + delta[2]])
        add_len+=3+ delta[2]
    n += delta[2]

# fix_to_zero_table: addr 3bytes 3*n
r_fix = bytearray()
for f in fix_to_zero_table:
    r_fix.extend(((f+application_addr)&0x00ffffff).to_bytes(3, byteorder='little', signed=False))
    
    
#x_off 3bytes,y_off 4bytes,n 2bytes,[(s,n) 4bytes/s 2bytes] 
reloc_diff = bytearray()
for i in merged_off_list:
    x_off = (i[0]).to_bytes(3, byteorder='little', signed=True)
    x_array=bytearray()
    for j in i[1]:
        y_off = j[0] if j[0]>=0 else j[0] + 0x100000000
        y_off = y_off.to_bytes(4, byteorder='little', signed=False)
        y_array = bytearray()
        for k in j[1]:
            if k[1]==1:
                s = (k[0] + 0x8000).to_bytes(2, byteorder='little', signed=False)
                y_array.extend(s)
            else:
                s = k[0].to_bytes(2, byteorder='little', signed=False)
                n = k[1].to_bytes(2, byteorder='little', signed=False)
                y_array.extend(s)
                y_array.extend(n)
        x_array.extend(y_off)
        x_array.extend((6+len(y_array)).to_bytes(2, byteorder='little', signed=False))
        x_array.extend(y_array)
    reloc_diff.extend(x_off)
    reloc_diff.extend((5+len(x_array)).to_bytes(2, byteorder='little', signed=False))
    reloc_diff.extend(x_array)
 

# header base0 1 2 3*2bytes
# addr 2bytes, data 4bytes     
reloc_add = bytearray()
reloc_add_body = bytearray()
base0 = 0
base1 = 0
base2 = 0
for i in add_reloc:
    if i[0]>>16 == 0x0800:
        base0 +=1;
    elif i[0]>>16 == 0x0801:
        base1 +=1;
    else:
        base2 +=1;
        
    reloc_add_body.extend((i[0]&0xffff).to_bytes(2, byteorder='little', signed=False))
    reloc_add_body.extend(i[1].to_bytes(4, byteorder='little', signed=False))
    
reloc_add.extend(base0.to_bytes(2, byteorder='little', signed=False))
reloc_add.extend((base0 + base1).to_bytes(2, byteorder='little', signed=False))
reloc_add.extend((base0 + base1 + base2).to_bytes(2, byteorder='little', signed=False))
reloc_add.extend(reloc_add_body)
print("add_reloc_len",base0, base1,base2 ,len(add_reloc))


# flag
header[0:4] = (1).to_bytes(4, byteorder='little', signed=False)
# move length
header[4:8] = max_move.to_bytes(4, byteorder='little', signed=False)
# old bin length
header[8:12] = len(old_bin).to_bytes(4, byteorder='little', signed=False)
# deltas end
header[12:16] = (34+ len(r_deltas)).to_bytes(4, byteorder='little', signed=False)
# fix zero end
header[16:20] = (34+ len(r_deltas) + len(r_fix)).to_bytes(4, byteorder='little', signed=False)
# reloc diff end
header[20:24] = (34+ len(r_deltas) + len(r_fix) + len(reloc_diff)).to_bytes(4, byteorder='little', signed=False)
# reloc new length
header[24:26] = len(new_reloc_list).to_bytes(2, byteorder='little', signed=False)
# reloc move length
header[26:28] = reloc_max_save_len.to_bytes(2, byteorder='little', signed=False)

base0 = 0
base1 = 0
base2 = 0
for rel in old_reloc_list:
    if rel[0]>>16 == 0x0800:
        base0 +=1;
    elif rel[0]>>16 == 0x0801:
        base1 +=1;
    else:
        base2 +=1;
# reloc old length 0x08000000
header[28:30] = base0.to_bytes(2, byteorder='little', signed=False)
# reloc old length 0x08010000
header[30:32] = (base0 + base1).to_bytes(2, byteorder='little', signed=False)
# reloc old length 0x08020000
header[32:34] = (base0 + base1 + base2).to_bytes(2, byteorder='little', signed=False)



result.extend(header)
result.extend(r_deltas)
result.extend(r_fix)
result.extend(reloc_diff)
result.extend(reloc_add)



fresult_bin = open("result.bin", 'wb')
fresult_bin.write(result)
fresult_bin.close()
print("deltas", len(r_deltas),add_len,copy_len)
print("r_fix", len(r_fix))
print("reloc_diff", len(reloc_diff))
print("reloc_add", len(reloc_add))
print("result", len(result))

# relocatable : addr 2bytes, data 4bytes, 6*n bytes
old_relocatable = bytearray()
for rel in old_reloc_list:
    old_relocatable.extend((rel[0]&0xFFFF).to_bytes(2, byteorder='little', signed=False))
    old_relocatable.extend(rel[1].to_bytes(4, byteorder='little', signed=False))
fold_relocatable = open("old_reloc.bin", 'wb')
fold_relocatable.write(old_relocatable)
fold_relocatable.close()

#print("old_relocatable",len(old_relocatable))

new_relocatable = bytearray()
for rel in new_reloc_list:
    new_relocatable.extend((rel[0]&0xFFFF).to_bytes(2, byteorder='little', signed=False))
    new_relocatable.extend(rel[1].to_bytes(4, byteorder='little', signed=False))
fnew_relocatable = open("new_reloc.bin", 'wb')
fnew_relocatable.write(new_relocatable)
fnew_relocatable.close()

#print("new_relocatable",len(new_relocatable))
#for i in range(len(new_reloc_list)):
#    print(i,"%x"%new_reloc_list[i][0],"%x"%new_reloc_list[i][1])

