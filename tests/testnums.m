clear a
a = MatShare;
nums = 10000;
lents = 0;
disp(getmem);
tic
for i = 1:nums
	a.data = i;
	x = a.data;
 	disp(a.data);
 	timestr = sprintf('%d/%d\n', i, nums);
 	fprintf([repmat('\b',1,lents) timestr]);
 	lents = numel(timestr);
end
toc
disp(getmem);