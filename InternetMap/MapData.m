//
//  MapData
//  InternetMap
//

#import "MapData.h"
#import "MapDisplay.h"
#import "Node.h"
#import "Lines.h"

@interface MapData ()
@property (strong, nonatomic) NSMutableDictionary* nodesByAsn;
@property (strong, nonatomic) NSMutableArray* connections;
@property (strong, nonatomic) NSString* filename;
@end

@implementation MapData

-(Node*)nodeAtIndex:(NSUInteger)index {
    return [self.nodes objectAtIndex:index];
}

-(void)loadFromFile:(NSString*)filename {
    NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];
    
    NSString *fileContents = [NSString stringWithContentsOfFile:filename encoding:NSUTF8StringEncoding error:NULL];
    NSArray *lines = [fileContents componentsSeparatedByString:@"\n"];
    
    NSArray* header = [[lines objectAtIndex:0] componentsSeparatedByString:@"  "];
    int numNodes = [[header objectAtIndex:0] intValue];
    int numConnections = [[header objectAtIndex:1] intValue];
    
    self.nodes = [[NSMutableArray alloc] initWithCapacity:numNodes];
    self.nodesByAsn = [[NSMutableDictionary alloc] initWithCapacity:numNodes];

    for (int i = 0; i < numNodes; i++) {
        NSArray* nodeDesc = [[lines objectAtIndex:1 + i] componentsSeparatedByString:@" "];
        
        Node* node = [Node new];
        node.asn = [nodeDesc objectAtIndex:0];
        node.index = i;
        node.importance = [[nodeDesc objectAtIndex:1] floatValue];
        node.positionX = [[nodeDesc objectAtIndex:2] floatValue];
        node.positionY = [[nodeDesc objectAtIndex:3] floatValue];
        node.type = AS_UNKNOWN;
        
        [self.nodes addObject:node];
        [self.nodesByAsn setObject:node forKey:node.asn];
    }
    
    self.connections = [NSMutableArray new];
    
    for (int i = 0; i < numConnections; i++) {
        NSArray* connectionDesc = [[lines objectAtIndex:1 + numNodes + i] componentsSeparatedByString:@" "];
        
        Node* first = [self.nodesByAsn valueForKey:[connectionDesc objectAtIndex:0]];
        Node* second = [self.nodesByAsn valueForKey:[connectionDesc objectAtIndex:1]];
        
        if((first.importance > 0.01) && (second.importance > 0.01)) {
            [self.connections addObject:[NSNumber numberWithInt:first.index]];
            [self.connections addObject:[NSNumber numberWithInt:second.index]];
        }
    }
    
    NSLog(@"load : %.2fms", ([NSDate timeIntervalSinceReferenceDate] - start) * 1000.0f);
}




-(void)loadFromAttrFile:(NSString*)filename {
    NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];

    NSDictionary *asTypeDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                [NSNumber numberWithInt:AS_UNKNOWN], @"abstained",
                                [NSNumber numberWithInt:AS_T1], @"t1",
                                [NSNumber numberWithInt:AS_T2], @"t2",
                                [NSNumber numberWithInt:AS_COMP], @"comp",
                                [NSNumber numberWithInt:AS_EDU], @"edu",
                                [NSNumber numberWithInt:AS_IX], @"ix",
                                [NSNumber numberWithInt:AS_NIC], @"nic",
                                nil];

    NSString *fileContents = [NSString stringWithContentsOfFile:filename encoding:NSUTF8StringEncoding error:NULL];
    NSArray *lines = [fileContents componentsSeparatedByString:@"\n"];

    for(NSString *line in lines) {
        NSArray* asDesc = [line componentsSeparatedByString:@"\t"];
        
        Node* node = [self.nodesByAsn objectForKey:[asDesc objectAtIndex:0]];
        if(node){
            node.type = [[asTypeDict objectForKey: [asDesc objectAtIndex:7]] intValue];
            node.typeString = [asDesc objectAtIndex:7];
            node.textDescription = [asDesc objectAtIndex:1];
        }
    }
    
    
    NSLog(@"attr load : %.2fms", ([NSDate timeIntervalSinceReferenceDate] - start) * 1000.0f);
}


-(void)updateDisplay:(MapDisplay*)display {
    NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];
    display.numNodes = self.nodes.count;
    [self.visualization updateDisplay:display forNodes:self.nodes];
    
    Lines* lines = [[Lines alloc] initWithLineCount:self.connections.count / 2];
    
    [lines beginUpdate];
    for(int i = 0; i < (self.connections.count - 1); i = i + 2) {
        Node* a = [self.nodes objectAtIndex:[[self.connections objectAtIndex:i] intValue]];
        Node* b = [self.nodes objectAtIndex:[[self.connections objectAtIndex:i+1] intValue]];
        
        float lineImportanceA = MAX(a.importance - 0.01f, 0.0f) * 0.5f;
        UIColor* lineColorA = [UIColor colorWithRed:lineImportanceA green:lineImportanceA blue:lineImportanceA alpha:1.0];
        
        float lineImportanceB = MAX(b.importance - 0.01f, 0.0f) * 0.5f;
        UIColor* lineColorB = [UIColor colorWithRed:lineImportanceB green:lineImportanceB blue:lineImportanceB alpha:1.0];
        
        [lines updateLine:(i / 2) withStart:[self.visualization nodePosition:a] startColor:lineColorA end:[self.visualization nodePosition:b]  endColor:lineColorB];
    }
    [lines endUpdate];
    
    display.lines = lines;
        
    NSLog(@"update display : %.2fms", ([NSDate timeIntervalSinceReferenceDate] - start) * 1000.0f);
}

@end
