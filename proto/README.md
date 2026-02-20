Run the grpc client on your local machine to test the grpc server

1. 
Install C++ grpc library
Mac:
`brew install grpc`

Windows:
idk ask chatgpt

2.
Add grpc to pkg-config
happened automatically for me

verify with `pkg-config --libs grpc`

3.
Make proto files
`rm -rf *.pb.h && rm -rf *.pb.cc`
`cd .. && make proto`
`cd proto`

4.
Make grpc client
`make`

5. Run client
`./grpc_client`
Notes: the client won't tell you if it can't access the grpc server. You should see either error messages or return data from readdirectory
