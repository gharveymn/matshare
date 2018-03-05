clear a
a = MatShare;
nums = 5000000;
lents = 0;
disp(getmem);
tic
for i = 1:nums
	a.data = i;
% 	timestr = sprintf('%d/%d\n', a.data, nums);
% 	fprintf([repmat('\b',1,lents) timestr]);
% 	lents = numel(timestr);
end
toc
disp(getmem);