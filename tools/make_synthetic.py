import struct
types=[(1,4,"car_speed","<f"),(2,24,"imu","<6f")]
b=b"FSAEDAQ\0"+struct.pack("<HB",1,len(types))
for i,l,n,f in types: b+=struct.pack("<BB",i,l)+n.encode().ljust(16,b"\0")+f.encode().ljust(16,b"\0")
for t in range(0,1000,10):
    b+=struct.pack("<BBBI",0xA5,2,24,t)+struct.pack("<6f",0,0,9.81,0,0,0)
    if t%20==0: b+=struct.pack("<BBBI",0xA5,1,4,t)+struct.pack("<f",t/100)
b+=bytes(4096)
open("/tmp/LOG00000.BIN","wb").write(b)
