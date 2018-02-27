clear x

MatShare.init();
nums = 100000;
lents = 0;
disp(getmem);
tic
for i = 1:nums
	
	MatShare.share(i);
	x = MatShare.fetch();
	
% 	disp(a.shared.data);
% 	timestr = sprintf('%d/%d', i, nums);
% 	fprintf([repmat('\b',1,lents) timestr]);
% 	lents = numel(timestr);
end
MatShare.free();
toc
disp(getmem);