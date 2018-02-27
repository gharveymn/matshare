a = MatShare;
nums = 100000;
lents = 0;
disp(getmem);
tic
for i = 1:nums
	a.data = i;
	x = a.data;
% 	disp(a.shared.data);
% 	timestr = sprintf('%d/%d', i, nums);
% 	fprintf([repmat('\b',1,lents) timestr]);
% 	lents = numel(timestr);
end
toc
disp(getmem);