# recdvb
Based on recdvb published on [sat](http://cgi1.plala.or.jp/~sat/?x=entry:entry160313-022002)  
  
## How to install
```bash
sudo apt install -y build-essential autoconf automake pkg-config
cd recdvb
bash autogen.sh
./configure --enable-b25
make
sudo make install
```
mirakurun  
```
command: recdvb --dev 0 --b25 --strip --lch <channel> - -
```
## 参考
https://qiita.com/ww24/items/0adc36c013511524da80
