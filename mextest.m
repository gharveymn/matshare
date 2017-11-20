addpath('res')
addpath('bin')

testvar = 'a';
matshare('share','a');
shared = matshare('get','testvar');
