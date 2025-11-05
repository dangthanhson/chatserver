project('chat-with-grpc', 'cpp', default_options: ['cpp_std=c++17'])

dep_proto = dependency('protobuf')
dep_grpc = dependency('grpc++')

executable('server', 'server.cpp', 'chatservice.grpc.pb.cc', 'chatservice.pb.cc', dependencies : [dep_proto, dep_grpc])
executable('client', 'client.cpp', 'chatservice.grpc.pb.cc', 'chatservice.pb.cc', dependencies : [dep_proto, dep_grpc])

# test('simple test', executable('unittest', 'unittest.cpp', dependencies : [dep_proto, dep_grpc]))
