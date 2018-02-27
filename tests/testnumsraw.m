clear data

obj.INIT     = uint8(0);
obj.CLONE    = uint8(1);
obj.ATTACH   = uint8(2);
obj.DETACH   = uint8(3);
obj.FREE     = uint8(4);
obj.FETCH    = uint8(5);
obj.COMPARE  = uint8(6);
obj.COPY     = uint8(7);
obj.DEEPCOPY = uint8(8);
obj.DEBUG    = uint8(9);

matshare_(obj.INIT,1);
nums = 10000000;
lents = 0;
disp(getmem);
tic
for i = 1:nums
	
	% place in shm
	% set callback
	matshare_(obj.CLONE, i);
	
	% assign to x
	x = matshare_(obj.FETCH);	
	
% 	disp(a.shared.data);
% 	timestr = sprintf('%d/%d', i, nums);
% 	fprintf([repmat('\b',1,lents) timestr]);
% 	lents = numel(timestr);
end
toc
disp(getmem);
matshare_(obj.FREE);