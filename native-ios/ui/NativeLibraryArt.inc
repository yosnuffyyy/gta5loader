/* Original procedural library artwork: no downloaded launcher assets, fonts,
   artwork decoder, timer, animation or retained full-resolution bitmap. */
@interface NativeLibraryArt : UIView
@end
@implementation NativeLibraryArt
- (instancetype)init {if((self=[super init])){self.opaque=YES;self.contentMode=UIViewContentModeRedraw;self.layer.cornerRadius=14;self.clipsToBounds=YES;self.accessibilityElementsHidden=YES;}return self;}
- (void)drawRect:(CGRect)rect {
 CGContextRef c=UIGraphicsGetCurrentContext();CGFloat w=self.bounds.size.width,h=self.bounds.size.height;
 [[UIColor colorWithRed:.10 green:.115 blue:.13 alpha:1] setFill];UIRectFill(self.bounds);
 CGContextSetFillColorWithColor(c,[UIColor colorWithRed:.84 green:.65 blue:.26 alpha:1].CGColor);CGContextFillEllipseInRect(c,CGRectMake(w*.70,h*.12,h*.58,h*.58));
 [[UIColor colorWithRed:.055 green:.065 blue:.08 alpha:1] setFill];
 for(unsigned i=0;i<22;i++){CGFloat x=w*i/21.,bh=h*(.18+((i*17)%11)*.027);UIRectFill(CGRectMake(x,h-bh,w/19.,bh));}
 [[UIColor colorWithRed:.16 green:.17 blue:.18 alpha:1] setFill];
 UIBezierPath *road=[UIBezierPath bezierPath];[road moveToPoint:CGPointMake(w*.42,h)];[road addLineToPoint:CGPointMake(w*.77,h*.54)];[road addLineToPoint:CGPointMake(w*.79,h*.54)];[road addLineToPoint:CGPointMake(w*.78,h)];[road closePath];[road fill];
 NSDictionary *small=@{NSFontAttributeName:[UIFont systemFontOfSize:11 weight:UIFontWeightSemibold],NSForegroundColorAttributeName:[UIColor colorWithWhite:.8 alpha:1],NSKernAttributeName:@2};
 BOOL compact=h<130;
 [@"YOUR LIBRARY / LOS SANTOS" drawAtPoint:CGPointMake(22,compact?12:20) withAttributes:small];
 [@"GRAND THEFT AUTO V" drawInRect:CGRectMake(22,compact?34:49,w-44,compact?50:100) withAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:compact?28:(w<400?25:34) weight:UIFontWeightHeavy],NSForegroundColorAttributeName:UIColor.whiteColor}];
 if(!compact)[@"STORY  ·  OFFLINE FREE ROAM" drawAtPoint:CGPointMake(22,h-34) withAttributes:small];
}
@end
