nums = 50000;
lents = 0;
tic
for i = 1:nums
	a = matshare(i);
	b = a.data;
% 	timestr = sprintf('%d/%d\n', a.data, nums);
% 	fprintf([repmat('\b',1,lents) timestr]);
% 	lents = numel(timestr);
end
toc