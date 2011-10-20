#import <UIKit/UIKit.h>


@protocol SslDelegateProtocol

// The delegate can receive text notifications about status and error messages.
- (void) logDebugMessage:(NSString*)message;

// The delegate can receive data from the SSL connection.
- (void) handleData:(NSString*)data;

@end
