project('chat-with-grpc', 'cpp', default_options: ['cpp_std=c++17'])

dep_proto = dependency('protobuf')
dep_grpc = dependency('grpc++')
#incdir = include_directories('include')

executable('server', 'server/server.cpp', 'proto/chatservice.grpc.pb.cc', 'proto/chatservice.pb.cc', dependencies : [dep_proto, dep_grpc])
executable('client', 'client/client.cpp', 'proto/chatservice.grpc.pb.cc', 'proto/chatservice.pb.cc', dependencies : [dep_proto, dep_grpc])

test('simple test', executable('unittest', 'unittest/unittest.cpp', 'proto/chatservice.grpc.pb.cc', 'proto/chatservice.pb.cc', dependencies : [dep_proto, dep_grpc]))
