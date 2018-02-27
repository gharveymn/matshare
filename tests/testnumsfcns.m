clear x

msh_init();
nums = 1000000;
lents = 0;
disp(getmem);
tic
for i = 1:nums
	
	msh_share(i);
	x = msh_fetch();
	
% 	disp(a.shared.data);
% 	timestr = sprintf('%d/%d', i, nums);
% 	fprintf([repmat('\b',1,lents) timestr]);
% 	lents = numel(timestr);
end
msh_free();
toc
disp(getmem);