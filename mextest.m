addpath('res')
addpath('out')

testvar = 'a';
matshare('share','a');
shared = matshare('get','testvar');
