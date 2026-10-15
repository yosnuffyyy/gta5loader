/* UIKit owns event ordering on the main queue. The bridge copies fixed state
   under a short mutex; controller handlers never access Wasm or the GPU. */
#include "native_input.h"
#include "touch_layout.h"
@interface NativeTouchArt : UIView {
    NSArray<NSString *> *_labels;
    NSDictionary *_smallText,*_largeText;
    UIColor *_idleColor,*_heldColor;
}
@property(nonatomic,assign) NITouches *touchState;
@end
@implementation NativeTouchArt
- (instancetype)init {if((self=[super init])){NSMutableArray *labels=[NSMutableArray array];for(unsigned i=0;i<NI_CONTROL_COUNT;i++)[labels addObject:[NSString stringWithUTF8String:ni_controls[i].label]];_labels=[labels copy];_smallText=@{NSFontAttributeName:[UIFont systemFontOfSize:11 weight:UIFontWeightSemibold],NSForegroundColorAttributeName:UIColor.whiteColor};_largeText=@{NSFontAttributeName:[UIFont systemFontOfSize:12 weight:UIFontWeightSemibold],NSForegroundColorAttributeName:UIColor.whiteColor};_idleColor=[UIColor colorWithWhite:.12 alpha:.7];_heldColor=[UIColor colorWithWhite:.8 alpha:.7];}return self;}
- (void)drawRect:(CGRect)rect {
    NITouches *t=self.touchState;if(!t)return;CGContextRef g=UIGraphicsGetCurrentContext();
    for(unsigned c=0;c<NI_CONTROL_COUNT;c++){
        float x,y;ni_center(t,c,&x,&y);float r=ni_radius(t,c);BOOL held=NO;
        for(unsigned i=0;i<NI_TOUCHES;i++)if(t->points[i].used&&t->points[i].inside&&t->points[i].control==c)held=YES;
        CGContextSetFillColorWithColor(g,(held?_heldColor:_idleColor).CGColor);
        CGContextFillEllipseInRect(g,CGRectMake(x-r,y-r,r*2,r*2));CGContextSetStrokeColorWithColor(g,UIColor.whiteColor.CGColor);CGContextSetLineWidth(g,1);CGContextStrokeEllipseInRect(g,CGRectMake(x-r,y-r,r*2,r*2));
        NSString *s=_labels[c];NSDictionary *a=c<2?_largeText:_smallText;CGSize size=[s sizeWithAttributes:a];[s drawAtPoint:CGPointMake(x-size.width/2,y-size.height/2) withAttributes:a];
        if(c<2)for(unsigned i=0;i<NI_TOUCHES;i++)if(t->points[i].used&&t->points[i].control==c){float dx=t->points[i].x-x,dy=t->points[i].y-y,m=hypotf(dx,dy);if(m>r){dx*=r/m;dy*=r/m;}CGContextSetFillColorWithColor(g,UIColor.whiteColor.CGColor);CGContextFillEllipseInRect(g,CGRectMake(x+dx-r*.2,y+dy-r*.2,r*.4,r*.4));}
    }
}
@end
@interface NativeGameControls : UIView {
    NITouches _touchState;
    UITouch *_touchOwners[NI_TOUCHES];
}
@property(nonatomic,strong) UIView *gameSurface;
@property(nonatomic,strong) NativeTouchArt *art;
@property(nonatomic,strong) UIButton *settingsButton;
@property(nonatomic,assign) BOOL playing;
@property(nonatomic,assign) float overlayOpacity;
@property(nonatomic,assign) float stickScale;
- (instancetype)initWithSurface:(UIView *)surface;
- (void)updateSource;
- (void)clearTouches;
@end
@implementation NativeGameControls
- (instancetype)initWithSurface:(UIView *)surface {if((self=[super initWithFrame:CGRectZero])){
    self.backgroundColor=UIColor.blackColor;self.multipleTouchEnabled=YES;self.gameSurface=surface;surface.userInteractionEnabled=NO;[self addSubview:surface];
    self.art=[[NativeTouchArt alloc]init];self.art.opaque=NO;self.art.backgroundColor=UIColor.clearColor;self.art.userInteractionEnabled=NO;self.art.touchState=&_touchState;[self addSubview:self.art];self.overlayOpacity=.55;self.stickScale=1;
    self.settingsButton=[UIButton buttonWithType:UIButtonTypeSystem];[self.settingsButton setTitle:@"Settings" forState:UIControlStateNormal];self.settingsButton.backgroundColor=[UIColor colorWithWhite:0 alpha:.65];self.settingsButton.layer.cornerRadius=8;self.settingsButton.accessibilityLabel=@"Game settings and exit fullscreen";[self addSubview:self.settingsButton];
 }return self;}
- (void)layoutSubviews {
    [super layoutSubviews];NIRect fit=ni_aspect_fit(self.bounds.size.width,self.bounds.size.height);self.gameSurface.frame=CGRectMake(fit.x,fit.y,fit.w,fit.h);self.art.frame=self.bounds;
    UIEdgeInsets s=self.safeAreaInsets;CGRect safe=UIEdgeInsetsInsetRect(self.bounds,s);CGRect area=CGRectIntersection(safe,self.gameSurface.frame);if(CGRectIsNull(area))area=safe;
    NIRect next={area.origin.x,area.origin.y,area.size.width,area.size.height};
    if(memcmp(&next,&_touchState.rect,sizeof(next))){[self clearTouches];_touchState.rect=next;}
    _touchState.scale=self.stickScale;self.settingsButton.frame=CGRectMake(CGRectGetMaxX(safe)-88,CGRectGetMinY(safe)+4,84,32);[self.art setNeedsDisplay];
}
- (void)setPlaying:(BOOL)playing {_playing=playing;[self clearTouches];[self updateSource];}
- (void)clearTouches {for(unsigned i=0;i<NI_TOUCHES;i++)_touchOwners[i]=nil;ni_touch_clear(&_touchState);ni_touch((NIState){0},false);[self.art setNeedsDisplay];}
- (void)updateSource {BOOL touch=self.playing&&ni_source()==NI_TOUCH;if(!touch)[self clearTouches];self.art.alpha=touch?self.overlayOpacity:0;/* Art never intercepts touches. The game container detects deliberate wake touches even while art is invisible. */self.art.userInteractionEnabled=NO;self.settingsButton.hidden=!self.playing;}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if(!self.playing)return;ni_touch(ni_touch_state(&_touchState),true);[self updateSource];
    for(UITouch *touch in touches){for(unsigned i=0;i<NI_TOUCHES;i++)if(!_touchOwners[i]){CGPoint p=[touch locationInView:self];_touchOwners[i]=touch;ni_touch_begin(&_touchState,i,p.x,p.y);break;}}
    ni_touch(ni_touch_state(&_touchState),false);[self.art setNeedsDisplay];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {if(!self.playing)return;for(UITouch *touch in touches)for(unsigned i=0;i<NI_TOUCHES;i++)if(_touchOwners[i]==touch){CGPoint p=[touch locationInView:self];ni_touch_move(&_touchState,i,p.x,p.y);}ni_touch(ni_touch_state(&_touchState),false);[self.art setNeedsDisplay];}
- (void)finishTouches:(NSSet<UITouch *> *)touches {for(UITouch *touch in touches)for(unsigned i=0;i<NI_TOUCHES;i++)if(_touchOwners[i]==touch){_touchOwners[i]=nil;ni_touch_end(&_touchState,i);}ni_touch(ni_touch_state(&_touchState),false);[self.art setNeedsDisplay];}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {[self finishTouches:touches];}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {[self finishTouches:touches];}
@end
static NIState NativeControllerState(GCExtendedGamepad *p){
    NIState s={.lx=p.leftThumbstick.xAxis.value,.ly=p.leftThumbstick.yAxis.value,.rx=p.rightThumbstick.xAxis.value,.ry=p.rightThumbstick.yAxis.value,.lt=p.leftTrigger.value,.rt=p.rightTrigger.value};
    if(p.buttonA.isPressed)s.buttons|=NI_CROSS;if(p.buttonB.isPressed)s.buttons|=NI_CIRCLE;if(p.buttonX.isPressed)s.buttons|=NI_SQUARE;if(p.buttonY.isPressed)s.buttons|=NI_TRIANGLE;
    if(p.leftShoulder.isPressed)s.buttons|=NI_L1;if(p.rightShoulder.isPressed)s.buttons|=NI_R1;if(p.leftThumbstickButton.isPressed)s.buttons|=NI_L3;if(p.rightThumbstickButton.isPressed)s.buttons|=NI_R3;
    if(p.buttonMenu.isPressed)s.buttons|=NI_START;if(p.buttonOptions.isPressed)s.buttons|=NI_SELECT;
    if(p.dpad.up.isPressed)s.buttons|=NI_UP;if(p.dpad.right.isPressed)s.buttons|=NI_RIGHT;if(p.dpad.down.isPressed)s.buttons|=NI_DOWN;if(p.dpad.left.isPressed)s.buttons|=NI_LEFT;return s;
}
