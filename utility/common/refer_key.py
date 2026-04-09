# example code to calculate the seed and key


class GenKey():
    def __init__(self,session,seed):
        self.seed=int(seed,16)      ## seed format '0xA1B2C3D4', format to hex
        self.session=session
        self.key=[]
        self.mask=''
        self.seedlist=[hex]
    
    def __sesscheck(self):
        if self.session == '10 02':
            self.mask=0x54424F35
        elif self.session=='10 03':
            self.mask=0x54424F58
        elif self.session=='10 04':
            self.mask=0x91E67F4A
        else:
            self.mask=0x00000000
     
    def __algorithm(self):
        calmask=self.mask
        calseed=self.seed
        for i in range(35):
            if calseed&0x80000000:
                calseed=calseed<<1
                calseed=calseed^calmask
            else:
                calseed=calseed<<1
        key='0x'
        key+=hex((calseed>>24)&0xFF)[2:4]
        key+=hex((calseed>>16)&0xFF)[2:4]
        key+=hex((calseed>>8)&0xFF)[2:4]
        key+=hex((calseed>>0)&0xFF)[2:4]
        return key

    def genkey(self):
        self.__sesscheck()
        self.key=self.__algorithm()
        return self.key
    

