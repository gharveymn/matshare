test_start = 1;
num_tests = 100000;

[X,Y] = meshgrid((1:8),(test_start:test_start+num_tests-1));
ipts = (test_start:test_start+num_tests-1)';
res = zeros(num_tests, 8);
resbeg = zeros(num_tests, 1);
resend = zeros(num_tests, 1);
for i = 1:num_tests
	[res(i,:),resbeg(i),resend(i)] = memhead(ipts(i));
end

scatter3(X(:),Y(:),res(:),'.');
hold on
%scatter(ipts,mod(resend-resbeg,64)/64*32,'.');
%scatter(ipts,mod(resbeg,16)/16*32,'.');
%scatter(ipts,mod(resend,16)/16*32,'.');
% ylim([0,32]);
hold off
