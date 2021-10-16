# ReactorBasedVPN
基于Reactor设计模式的http代理服务器

## 编译构建该代理服务器
```
$ cmake .
$ make
```

## http代理服务器的使用
‘’‘
./ReactorBasedVPN #服务端开启监听
’‘’
或者
‘’‘
nohup ./ReactorBasedVPN #在后台运行
’‘’

## 设置代理服务器ip以及端口8888为代理ip以及代理端口进行网页请求访问
代理服务器接收代理请求
<img width="1062" alt="WeChat0abe0f94797049d7c0c08b14c47759e5" src="https://user-images.githubusercontent.com/53205201/137583945-62ae93b7-9284-4b61-8c91-e177f1a1bc2f.png">

代理服务器服务器返回网页
<img width="1222" alt="WeChat58bbd5f53489f864ec79456d9c1002ee" src="https://user-images.githubusercontent.com/53205201/137583917-fcd6be07-a4d8-43da-b4ab-ff22c4808967.png">
