num_pts = 500;
num_frames = 1200;

theta = linspace(0, pi, num_pts);                  % polar angle
phi = linspace(0, 2*pi, num_pts);                 % azimuth angle

[phi,theta] = meshgrid(phi,theta);    % define the grid

degree = 6;
order = 1;
amplitude = 0.5;
radius = 5;

Ymn = legendre(degree,cos(theta(:,1)));
Ymn = Ymn(order+1,:)';
yy = repmat(Ymn, 1, num_pts) .* cos(order.*phi);

order = max(max(abs(yy)));

scale = [linspace(0,1,num_frames * 1/3) linspace(1,-1,num_frames * 2/3)];    % surface scaling (0 to 1 to -1)

frames = zeros(num_pts, num_pts, 3, num_frames);

matshare.examples.mshpoolstartup;
matshare.clear;
matshare.share('-n',           ...
	'theta', theta, ...
	'phi', phi,     ...
	'scale', scale, ...
	'yy', yy,       ...
	'frames', frames);
tic
parfor ii = 1:num_frames
	sh = matshare.fetch('-n');
	
	prho = radius + sh.scale.data(ii)*amplitude*sh.yy.data/order;
	pr = prho.*sin(sh.theta.data);

	sh.frames.data(:,:,1,ii) = pr.*cos(sh.phi.data);
	sh.frames.data(:,:,2,ii) = pr.*sin(sh.phi.data);
	sh.frames.data(:,:,3,ii) = prho.*cos(sh.theta.data);
end
timetaken = toc;
fprintf('Average FPS: %f\n', num_frames/timetaken);

matshare.clear;

frames = 0.*frames;
tic
for ii = 1:num_frames
	
	rho = radius + scale(ii)*amplitude*yy/order;
	
	r = rho.*sin(theta);
	frames(:,:,1,ii) = r.*cos(phi);
	frames(:,:,2,ii) = r.*sin(phi);
	frames(:,:,3,ii) = rho.*cos(theta);
end
timetaken = toc;
fprintf('\nAverage FPS: %f\n', num_frames/timetaken);


