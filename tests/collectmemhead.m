num_tests = 64;

[X,Y] = meshgrid((1:16),(1:num_tests));
ipts = (1:num_tests)';
res = zeros(num_tests, 16);
resbeg = zeros(num_tests, 1);
resend = zeros(num_tests, 1);
j = ipts(randperm(numel(ipts)));
for i = 1:num_tests
	[res(j(i),:),resbeg(j(i)),resend(j(i))] = memhead(j(i));
end

scatter(ipts,res(:,15),'.');
hold on
scatter(ipts,mod(resend-resbeg,64)/64*32,'.');
%scatter(ipts,mod(resbeg,16)/16*32,'.');
%scatter(ipts,mod(resend,16)/16*32,'.');
ylim([0,32]);
hold off
