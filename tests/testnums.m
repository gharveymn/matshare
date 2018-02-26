a = MatShare;
nums = 1000000;
lents = 0;
for i = 1:nums
	a.shared.data = i;
	timestr = sprintf('%d/%d', a.shared.data, nums);
	fprintf([repmat('\b',1,lents) timestr]);
	lents = numel(timestr);
end