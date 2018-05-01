clear x

nums = 5000000;
lents = 0;
disp(getmem);
tic
for i = 1:nums
	
	data = i;
	x = data;
	
% 	disp(a.shared.data);
% 	timestr = sprintf('%d/%d', i, nums);
% 	fprintf([repmat('\b',1,lents) timestr]);
% 	lents = numel(timestr);
end
toc
disp(getmem);