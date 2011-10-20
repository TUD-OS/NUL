#import <UIKit/UIKit.h>

@class IphoneClientViewController;

@interface iphoneClientAppDelegate : NSObject <UIApplicationDelegate>
{
    UIWindow* window;
    IphoneClientViewController* viewController;
}

@property (nonatomic, retain) IBOutlet UIWindow* window;
@property (nonatomic, retain) IBOutlet IphoneClientViewController* viewController;

@end

